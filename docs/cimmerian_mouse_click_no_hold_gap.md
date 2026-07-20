# Cimmerian — Gap: `MouseClickEvent` presses and releases with no hold, missed by once-per-frame input polling

> **Trigger:** trying to extend `pharos-proto`'s visual-regression suite
> (`docs/pharos_visual_regression_feature_request.md`) to cover Explorer
> row selection — the state that populates `InspectorPanel`, itself not yet
> in that suite (`pharos-proto/docs/penumbra_iris_lustre_componentization_gaps_requirements.md`'s
> own "what's left open" list). A click-driven selection test was attempted
> and deliberately not kept; this is the first of two independent reasons it
> failed (the second, an X11-driver DPI mismatch, was `penumbra-proto`-side
> and has since been fixed there — unrelated to Cimmerian).
> **Status:** confirmed, reproducible, not started. Affects every
> `IEventInjector` backend identically — not platform-specific.
> **Environment:** found via `LinuxUinputEventInjector` against a real
> `pharos-proto` binary (KDE Plasma/KWin, `SDL_VIDEODRIVER=x11`), but the
> root cause is in `MouseClickEvent`'s own handling, reproduced by reading
> (not just running) all four injector backends — see §2.

---

## 1. What was observed

Driving a real click at a `pharos-proto` Explorer row via
`SEND(Cimmerian::Visual::MouseClick(x, y))`, then capturing a snapshot,
consistently showed the row's per-frame hover tint
(`theme.ColorRowHover`) but never its distinct selected-state gradient
fill — the fill `TreeRow::DrawContent` only draws once
`ExplorerPanel::selectedNode` has actually changed
(`pharos-proto/src/ui/explorer_panel.cpp`). The click was landing (the
hover tint proves the pointer position and window addressing were both
correct) but never registering as a full press-then-release from the
target application's point of view.

Root cause, traced to `pharos-proto/CLAUDE.md`'s own pre-existing note
about `cliclick` on macOS: `PlatformWindow` (`penumbra-proto`) polls
mouse-button state **once per frame**, not from a drained event queue. A
synthetic press and release delivered back-to-back with no gap between
them can both land inside the same polling interval, or so close together
that the widget's own edge-detection (down *this* frame, up next) never
observes a down-then-later-up sequence at all — indistinguishable, from
the application's side, from no click having happened. This is exactly
the failure mode `pharos-proto/CLAUDE.md` already documents for
`cliclick dd:`/`du:` (held press) versus an instantaneous click on macOS,
and for `ydotool`/`xdotool`'s own press-hold-release timing on Linux — but
neither Pharos nor any app using Cimmerian has a way to ask Cimmerian for
a *held* click, because the primitive itself doesn't exist.

---

## 2. Root cause in Cimmerian itself

`MouseClickEvent` (`include/cimmerian/visual/ui-event.hpp:13-17`) is a
single event carrying only `{x, y, button}` — no duration, no way to
separate press from release. Every `IEventInjector::Inject()`
implementation handles it as an unconditional, immediate down-then-up
pair, with no delay and no caller-visible way to insert one:

- `LinuxUinputEventInjector::Inject` (`src/visual/platform/linux-uinput-event-injector.cpp:211-215`):
  ```cpp
  else if constexpr (std::is_same_v<T, MouseClickEvent>) {
    this->MoveTo(e.x, e.y);
    const int btn = ButtonToEvdev(e.button);
    this->EmitKey(btn, 1);
    this->EmitKey(btn, 0);
  }
  ```
- `X11EventInjector::Inject` (`src/visual/platform/x11-event-injector.cpp:101-105`): same shape,
  `XTestFakeButtonEvent(..., True, ...)` immediately followed by
  `XTestFakeButtonEvent(..., False, ...)`.
- `Win32EventInjector::Inject` (`src/visual/platform/win32-event-injector.cpp:77-81`) and
  `MacOsEventInjector::Inject` (`src/visual/platform/macos-event-injector.cpp:44-`): same
  down/up-with-no-gap shape, platform APIs aside.

Contrast with `KeyPressEvent`/`KeyReleaseEvent`
(`ui-event.hpp:24-29`), which **are** already split into two separate
events — a caller that needs a held key can already do
`SEND(KeyPress(k)); SEND(Wait(15ms)); SEND(KeyRelease(k))`.
`pharos-proto/tests/visual/pharos_test_app.hpp`'s own `TypeText` does
exactly this (15ms between press and release per character) for keyboard
input. No equivalent split exists for the mouse — `MouseClickEvent` is
the *only* pointer-button primitive `EventSequence`/`SEND()` expose, so
there is no way to reproduce `TypeText`'s pattern for a click no matter
how a `VISUAL_TEST` body is written.

---

## 3. REQUIRED — split `MouseClickEvent` into press/release, mirroring `KeyPressEvent`/`KeyReleaseEvent`

### Proposed API

```cpp
// include/cimmerian/visual/ui-event.hpp, alongside MouseClickEvent (kept,
// unchanged, for callers that don't care about hold duration)
struct MouseButtonPressEvent {
  int x;
  int y;
  int button; // 1=left 2=mid 3=right
};
struct MouseButtonReleaseEvent {
  int button; // no x/y -- a release targets whatever button is currently
              // down, same convention KeyReleaseEvent already uses (no
              // re-specified position)
};

using UIEvent = std::variant<
    MouseMoveEvent,
    MouseClickEvent,
    MouseButtonPressEvent,
    MouseButtonReleaseEvent,
    MouseScrollEvent,
    KeyPressEvent,
    KeyReleaseEvent,
    WaitEvent>;

// EventSequence, alongside Click(...)
EventSequence& MouseButtonPress(int x, int y, int button = 1) {
  this->events.push_back(MouseButtonPressEvent {x, y, button});
  return *this;
}
EventSequence& MouseButtonRelease(int button = 1) {
  this->events.push_back(MouseButtonReleaseEvent {button});
  return *this;
}
```

`MouseClickEvent`'s existing behavior (and every existing `SEND(MouseClick(...))`
call site) is unchanged — this is a pure addition, not a breaking change.
A caller that needs a held click composes it explicitly, the same way
`TypeText` already composes `KeyPress`/`Wait`/`KeyRelease`:

```cpp
SEND(Cimmerian::Visual::EventSequence{}
    .MouseButtonPress(x, y)
    .Wait(50ms)
    .MouseButtonRelease());
```

### Required changes elsewhere

- All four `IEventInjector::Inject()` implementations gain two new
  `if constexpr` branches each: `MouseButtonPressEvent` does the
  `MoveTo` + button-down half of the existing `MouseClickEvent` case;
  `MouseButtonReleaseEvent` does the button-up half. `MouseClickEvent`'s
  own branch is untouched — it can optionally be reimplemented in terms
  of the two new ones internally, but that's an implementation detail,
  not a contract change.
- `docs/visual-regression-spec.md`'s event-type table (§ around line 60-69)
  gains the two new struct shapes, matching how `KeyPressEvent`/
  `KeyReleaseEvent` are already documented there.

### What unblocks in Pharos

`pharos-proto/tests/visual/pharos_toolbar.visual.test.cpp`'s own trailing
comment records that an Explorer-selection / `ColorFilterDropdown`-open
visual test was attempted and abandoned specifically because of this gap
(among others, see that comment for the second, since-fixed cause). A
held `MouseButtonPress`/`Wait`/`MouseButtonRelease` sequence would let
that test be re-attempted for real, and is the direct prerequisite for
getting `InspectorPanel`'s populated state into Pharos's automated visual
suite at all — it has no other way to reach a selected state, since
`InspectorPanel` only renders content once a real Explorer row selection
has landed.

---

## 4. Explicitly not requested

- **Configurable/automatic hold duration on `MouseClickEvent` itself**
  (e.g. a new `durationMs` field). Rejected in favor of the explicit
  press/release split above: a single event with an added timing
  parameter still couples "what to do" with "how long," where
  `KeyPressEvent`/`KeyReleaseEvent`'s existing split already establishes
  the pattern of leaving timing entirely to the caller via `Wait()`. Two
  primitives is also strictly more capable (drag gestures — press, move,
  release — become expressible for free, which a single timed-click event
  could never support).
- **Auto-injecting a default hold into every existing `MouseClickEvent`.**
  Would silently change timing behavior for every current caller (and
  slow down every existing `SEND(MouseClick(...))` in this repo's own
  test suite) to fix a problem only some target applications' polling
  models actually have. Additive-only, opt-in via the new events, is the
  smaller and safer change.
- **Detecting or working around "did the click actually land."** Related
  to, but distinct from, `docs/cimmerian_wayland_xtest_injection_gap.md`'s
  §4 self-check proposal (injection reaching the compositor at all) — this
  doc is about giving callers the *primitive* to hold a press, not about
  Cimmerian verifying an application-level effect occurred. Out of scope
  here.

---

## 5. What unblocks when this lands

Any target application that polls input state once per frame rather than
draining an event queue (`penumbra-proto`'s `PlatformWindow`, and
plausibly others) becomes reliably click-testable through Cimmerian for
the first time — today, `MouseClickEvent`'s instantaneous down/up can
silently no-op against such an application with no error, only a
snapshot that quietly reflects pre-click state (the same "false pass"
risk `docs/cimmerian_wayland_xtest_injection_gap.md` §3 already flagged
for a different reason). For `pharos-proto` specifically, this is the
direct unblock for the Explorer-row-selection test that motivated this
investigation, and by extension for finally covering `InspectorPanel`'s
populated state in automated visual regression.
