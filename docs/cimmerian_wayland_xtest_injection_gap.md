# Cimmerian — Gap: X11EventInjector's XTEST-based input silently no-ops under (at least some) real Wayland compositors

> **Trigger:** trying to use the new visual-regression extension
> (`docs/visual-regression-spec.md`) to drive `pharos-proto`'s UI for automated
> screenshot verification — the same job `pharos-proto/CLAUDE.md`'s "Verifying
> UI changes" section previously did by hand with macOS's `cliclick`/
> `screencapture`. This is the Linux side of that same need.
> **Status:** addressed, via both directions §4 proposed. `X11ScreenCapture`
> (the other half of the same extension) is unaffected and works correctly in
> this same environment.
> `LinuxUinputEventInjector` (`src/visual/platform/linux-uinput-event-injector.cpp`,
> `CIMMERIAN_VISUAL_PLATFORM=Linux-uinput`) shipped in an earlier session as
> the `/dev/uinput`-based alternative backend. `X11EventInjector` itself
> gained the self-check `Probe()`/`IsFunctional()` (made an explicit,
> deliberate call rather than an eager constructor side effect — see
> `docs/cimmerian_live_app_visual_testing_gap.md` gap 3). This session added
> `AutoLinuxEventInjector` (`CIMMERIAN_VISUAL_PLATFORM=Linux-auto`,
> `src/visual/platform/auto-linux-event-injector.cpp`), which combines both:
> starts on `X11EventInjector`, and falls back to `LinuxUinputEventInjector`
> automatically if `Probe()` finds XTEST non-functional on the current
> compositor. Note what isn't and can't be fixed: a compositor that chooses
> not to forward XTEST-over-XWayland input into its real input pipeline
> can't be made to by anything on the client side — `X11EventInjector`
> itself stays exactly as reliable (or not) as its host compositor's XTEST
> support; `Linux-auto`/`LinuxUinputEventInjector` route around that rather
> than changing it.
> **Environment:** KDE Plasma 6 (`kwin_wayland`) as the real Wayland
> compositor, app under test running under XWayland
> (`SDL_VIDEODRIVER=x11`), `libXtst` present, `XTestQueryExtension` reports
> XTEST 2.2 available.

## 1. What works

`X11ScreenCapture::Capture()` (`src/visual/platform/x11-screen-capture.cpp`,
plain `XGetImage` against a given window or the root) works correctly in this
environment. Cimmerian's own `test/visual.test.cpp` ("Scratch Window / looks
the same between runs") passes cleanly against the pre-existing golden in
`snapshots/scratch-window/` — a real pixel-for-pixel match, not a stub. This
is a solid, precise, no-external-process capture path — notably better for
automated verification than shelling out to a desktop screenshot utility
(no portal/D-Bus round trip, no permission prompt, deterministic given a raw
X11 window id).

## 2. What doesn't: `X11EventInjector`'s pointer/button injection

`X11EventInjector::Inject()` (`src/visual/platform/x11-event-injector.cpp`)
uses `XTestFakeMotionEvent`/`XTestFakeButtonEvent`/`XTestFakeKeyEvent`
exactly as `docs/visual-regression-spec.md`'s own Platform Considerations
table prescribes for X11 — this is the standard, correct approach and isn't
itself a bug. The gap is that the underlying assumption ("target the X11
server, XTEST reaches whatever's real") doesn't hold for at least one real,
current Wayland compositor: `XTestFakeMotionEvent` reports success (`Bool`
return `True`) but never actually moves the seat's pointer.

**Confirmed three independent ways, from least to most Cimmerian-specific,
each showing the identical result:**

1. **`xdotool mousemove`** (both `--window`-relative and plain absolute
   forms) — pointer position read back via `xdotool getmouselocation`
   afterward is unchanged.
2. **Raw hand-rolled Xlib/XTEST probe**, bypassing xdotool and Cimmerian
   entirely:
   ```cpp
   Bool ok = XTestFakeMotionEvent(dpy, -1, 400, 400, CurrentTime);
   // ok == True
   XFlush(dpy);
   // XQueryPointer(...) immediately after: position unchanged
   ```
3. **Cimmerian's own `X11EventInjector` class**, linked directly (not through
   the test macros), against the real desktop:
   ```cpp
   Cimmerian::Visual::X11EventInjector injector;
   injector.Inject(Cimmerian::Visual::MouseMoveEvent{700, 700});
   // XQueryPointer before/after: identical position
   injector.Inject(Cimmerian::Visual::MouseClickEvent{800, 400, 1});
   // still identical position afterward
   ```

All three reach the same X server, use the same XTEST primitives, and
produce the same null result. `XTestFakeButtonEvent` at the (unmoved) current
pointer position also returns success with no observable effect. This isn't
an xdotool quirk or a Cimmerian bug in how it calls XTEST — the call is
correct and matches the extension's documented contract; the compositor
simply isn't forwarding XTEST-synthesized input into its real Wayland input
pipeline for windows in this session (a known category of issue — several
Wayland compositors implement XTEST only partially for XWayland clients, for
security/design reasons; this doesn't appear to be reliably detectable up
front short of doing exactly the kind of before/after `XQueryPointer` check
above).

## 3. Why this matters for the extension's own design

`IEventInjector::Inject()` returns `void` (`include/cimmerian/visual/
i-event-injector.hpp`) — there's no way for a caller (or `VisualTestRunner`)
to know injection silently failed. A `VISUAL_TEST` using `SEND(MouseClick(...))`
against a compositor like this one won't error — it'll just silently test
against whatever state the UI was already in before the (no-op) click,
which is a false-pass footgun worse than an outright failure would be: the
snapshot comparison can still succeed (correctly reproducing the *pre-click*
state every run) while asserting nothing about the actual interaction.

## 4. Proposed fix

Not proposing a specific implementation here (that's a real design decision,
same spirit as this ecosystem's other feature-request docs) — flagging the
gap and the two directions that seem most promising, for whoever picks this
up:

- **A `/dev/uinput`-based Linux injector** (same mechanism `ydotool`/`libei`
  use) as an alternative to `X11EventInjector`, selectable via
  `CIMMERIAN_VISUAL_PLATFORM` (e.g. a new `Linux-uinput` value alongside the
  existing `X11`) or auto-fallback. This operates below both X11 and
  Wayland at the kernel input-device layer, so it isn't subject to a
  compositor's XTEST-forwarding behavior at all — the same reasoning that
  makes it the standard recommended fallback for `xdotool`-doesn't-work
  Wayland sessions. Trade-off: needs `/dev/uinput` access (root, or a
  udev rule granting the `input` group — not always available in a sandboxed
  environment).
- **A self-check at `X11EventInjector` construction or first use**: do the
  same before/after `XQueryPointer` sanity check this investigation did by
  hand, and surface a clear error (or a queryable "is injection actually
  working" flag) rather than silently succeeding at the protocol level while
  doing nothing. Cheaper than a whole new backend, and valuable even once a
  uinput backend exists (detecting "XTEST is present but non-functional" is
  useful signal regardless of what the fallback ends up being).

## 5. What this doc is not

Not a report that Cimmerian's visual-regression extension is broken —
`X11ScreenCapture` is solid, and `X11EventInjector`'s implementation matches
its own spec correctly. This is specifically "XTEST-over-XWayland isn't a
universally reliable input-injection mechanism on Wayland," discovered while
trying to use the extension for its intended purpose (driving `pharos-proto`
through pointer clicks for automated snapshot verification) in a real,
unremarkable KDE Plasma desktop session — not a contrived edge case.
