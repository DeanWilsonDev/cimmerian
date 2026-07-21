# Cimmerian — Proposal: reach a screen without platform-level input injection

> **Trigger:** repeated live-desktop friction with using
> `SEND(MouseClick(...))`/`SEND(MouseMove(...))` sequences purely to
> *navigate* a consumer app to the screen under test, before any snapshot
> is even taken — on top of the reliability problems already filed
> (`docs/cimmerian_wayland_xtest_injection_gap.md`,
> `docs/cimmerian_uinput_no_functional_check_gap.md`,
> `docs/cimmerian_mouse_click_no_hold_gap.md`), `docs/
> cimmerian_live_app_visual_testing_gap.md` gaps 3 and 4 document that both
> injector backends are unscoped, global OS input with real side effects
> (cursor hijacking, focus races) on a live multi-window desktop. Using
> that same mechanism just to click through menus/tabs/routes to reach a
> screen compounds every one of those risks for a step that has nothing to
> do with the thing actually under test — the resulting *screen's*
> appearance, not the click that got there.
> **Status:** implemented. Proposal A landed as `include/cimmerian/visual/
> navigation-driver.hpp` (`NavigateFn`/`ActiveNavigationDriver`) and the
> `NAVIGATE` macro. Proposal B's Cimmerian-owned half landed as
> `VISUAL_DESCRIBE_COMPONENT` plus a `void*`-window-handle overload of
> `ASSERT_SNAPSHOT`; `MountComponent<T>` itself remains consumer-supplied
> as originally scoped (see §5) — `test/visual.test.cpp`'s
> `ScratchComponentHost` demonstrates the shape a real one would take.

---

## 1. Problem

`VISUAL_DESCRIBE`/`SEND`/`ASSERT_SNAPSHOT`
(`docs/visual-regression-spec.md`) has exactly one way to change what's on
screen: inject a real OS input event via `IEventInjector` and let the
consumer app's own UI handle it. That's the right tool for testing an
*interaction* (does this button show a hover state, does this click
change this row's fill) — the input genuinely is the thing under test.

It's the wrong tool for *navigation* — clicking a nav tab, opening a menu,
switching a route — where the input sequence is just plumbing to get the
window into the state a snapshot is wanted of. Every navigation-by-click
sequence pays the full cost documented in the existing gap docs (real
cursor movement, unscoped screen-absolute coordinates, focus-loss races,
per-platform injector flakiness, `MouseClickEvent`'s
press/release-in-one-frame gap) for zero test value — nothing about
*whether the nav click itself works* is being asserted, only what the
destination screen looks like once there.

## 2. Proposal A — a programmatic navigation hook, separate from `IEventInjector`

Add a second, optional way for a `VISUAL_DESCRIBE` block to reach a
screen: a consumer-supplied function that jumps the app directly to a
named screen/route/state, bypassing the UI entirely. `SEND`/`IEventInjector`
remains exactly as-is for tests that are actually about an interaction.

```cpp
// include/cimmerian/visual/navigation-driver.hpp (illustrative — new file)
namespace Cimmerian::Visual {

// Consumer-supplied hook that puts the app under test into a named
// screen/state directly (e.g. by calling the app's own router/view-model
// API in-process, or sending it an out-of-band IPC/debug command) rather
// than by simulating the clicks a real user would make to get there.
// Opaque string key - Cimmerian doesn't know or care what the consumer's
// screens are called; it's whatever the consumer's own navigation system
// uses (route name, enum, panel id, ...).
using NavigateFn = std::function<void(const std::string& screenKey)>;

class ActiveNavigationDriver {
public:
  static ActiveNavigationDriver& GetInstance();

  ActiveNavigationDriver(const ActiveNavigationDriver&) = delete;
  ActiveNavigationDriver& operator=(const ActiveNavigationDriver&) = delete;

  void Set(NavigateFn fn) { this->navigate = std::move(fn); }

  // Throws / TEST_LOG_ERRORs with a clear message if no navigator was
  // registered - this is opt-in, not a silent no-op.
  void NavigateTo(const std::string& screenKey);

private:
  ActiveNavigationDriver() = default;
  NavigateFn navigate;
};

} // namespace Cimmerian::Visual
```

New macro alongside `SEND`/`WAIT`:

```cpp
VISUAL_DESCRIBE("Settings Panel", settingsPanelWindow, {
    VISUAL_TEST("default state looks correct", {
        NAVIGATE("settings");        // jumps straight there, no clicks
        WAIT(50ms);
        ASSERT_SNAPSHOT("default-state");
    });

    VISUAL_TEST("hover on save button", {
        NAVIGATE("settings");
        SEND(MouseMove(200, 340));   // the actual interaction under test
        WAIT(16ms);
        ASSERT_SNAPSHOT("save-button-hover");
    });
});
```

For an in-process consumer (links Cimmerian directly, same binary), the
`NavigateFn` is typically a direct call into the app's own router/view
model. For a separately-launched app (the `WaitForWindowByTitle`/
`WaitForWindowByPid` case in `docs/cimmerian_live_app_visual_testing_gap.md`
gap 2), it would need to go over some out-of-band channel the consumer
already has reasons to expose for testing anyway (a debug IPC socket, a
CLI arg for initial screen, etc.) — Cimmerian only defines the hook shape,
not the transport, the same way `IEventInjector`/`IScreenCapture` leave
the platform mechanism to the implementation.

### What unblocks

Every visual test's setup step (get to screen X) stops depending on
click-injection reliability, cursor side effects, or focus races. Only
tests that are actually exercising an interaction still touch
`IEventInjector` at all — which narrows exactly where the known injection
risks (gaps 3/4, the uinput no-op gap, the no-hold gap) can still bite.

## 3. Proposal B — component-level snapshot tests

A stronger version of the same fix: skip full-app navigation entirely by
mounting a single component into its own dedicated host window, with
state/props set directly in test code, then snapshotting just that
component. No click, no navigation hook, no full app under test at all —
the equivalent of what `test/visual.test.cpp`'s inline scratch window
already does for whole-window tests, one level more granular.

```cpp
// Illustrative - shape only, not a final API.
VISUAL_DESCRIBE_COMPONENT("SaveButton", {
    VISUAL_TEST("default", {
        auto host = MountComponent<SaveButton>(SaveButtonProps{
            .label = "Save",
            .enabled = true,
        });
        WAIT(16ms);
        ASSERT_SNAPSHOT("default", host.WindowHandle());
    });

    VISUAL_TEST("disabled", {
        auto host = MountComponent<SaveButton>(SaveButtonProps{
            .label = "Save",
            .enabled = false,
        });
        WAIT(16ms);
        ASSERT_SNAPSHOT("disabled", host.WindowHandle());
    });
});
```

`MountComponent<T>` is consumer-supplied (Cimmerian can't know the
consumer's component/widget model), but its contract is simple: construct
a component with given props/state, host it in a dedicated,
appropriately-sized window, and return something exposing that window's
native handle — everything downstream (`IScreenCapture`, `SnapshotStore`,
`image-diff`, `ASSERT_SNAPSHOT`) is unchanged, since all of it already
only cares about a `void*` window handle, not how it got populated. The
snapshot directory layout (`snapshots/<group>/<test>/<label>.golden.png`)
needs no changes either — a component group is just another `<group-name>`
entry.

For components whose visual states are themselves interaction-driven
(hover, pressed, focus-ring), `SEND` against the host window is still
appropriate and low-risk in a way it isn't against a live multi-window
desktop app: the host window is dedicated, disposable, and (like
`test/visual.test.cpp`'s scratch window) the only thing on screen the
test cares about, so the unscoped-global-input risk in gap 4 has nothing
else to collide with.

### What unblocks

Most visual regressions are actually component-local (a button's hover
tint, a row's selected-state fill, a panel's default layout) — testing
them doesn't need a running instance of the whole app, doesn't need
window lookup, doesn't need navigation of any kind, and removes almost
all the platform-injection surface area from the majority of the suite.
Full-app/live-app tests remain valuable for layout composition and
cross-component interaction, but stop being the only option for
testing an individual component's appearance.

## 4. How the two relate

They're independent and additive, not alternatives:

- **Component-level tests (B)** are the right default for anything whose
  correctness lives in one component — cheapest to write, least input
  surface, least flaky.
- **The navigation hook (A)** is for full-app/live-app tests where the
  thing under test is genuinely a composed screen (does this panel lay
  out correctly with real sibling components, does this route's screen
  look right) — it removes the click-to-navigate tax from that case
  without requiring every such test to become a component test.
- Real `SEND()` click/key injection remains reserved for what it's
  actually good at: asserting an interaction's visual effect, ideally
  against a dedicated/scratch window in either style, per gap 4's existing
  guidance.

## 5. Explicitly not requested

- **Removing `IEventInjector`/platform input entirely** — it's still the
  only way to test that an interaction (not just a static state) renders
  correctly; both proposals narrow its usage, neither eliminates it.
- **A full declarative/virtual-DOM-style component test renderer** — proposal
  B still renders through the consumer's real windowing/graphics stack via
  a real (if disposable) OS window; it's not proposing an in-memory/
  headless render path.
- **Specifying `MountComponent<T>`'s exact API or `NavigateFn`'s transport
  for out-of-process consumers** — both need a short design pass with a
  real consumer in mind (`Amanuensis`, `pharos-proto`, `penumbra-proto`)
  before implementation; the shapes above are illustrative only.
