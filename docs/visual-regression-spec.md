# Visual Regression Testing Extension — Spec

## Overview

This document specifies a visual snapshot testing extension for Cimmerian. The goal is to let tests drive a UI through a sequence of input events, capture screenshots at designated checkpoints, and compare them against stored "golden" images. This catches unintended visual regressions without needing to manually inspect the UI after every change.

Two execution modes:

- **Update mode** — runs the scenario, captures screenshots, writes them to disk as the new golden set. Used when you intentionally change the UI or are capturing for the first time.
- **Verify mode** (default) — runs the scenario, captures screenshots, compares each one pixel-for-pixel against the stored golden. Fails the test if any snapshot exceeds the configured diff threshold.

A third **review mode** emits an HTML report showing golden vs. actual vs. diff side-by-side so you can eyeball ambiguous failures before deciding whether to accept them.

---

## Where This Lives in the Codebase

The extension adds a new sub-namespace and a parallel set of headers, keeping the existing `TEST`/`DESCRIBE` system untouched. Visual tests are registered in their own registry alongside the standard one and are only executed when the visual runner is invoked.

```
include/cimmerian/
  visual/
    ui-event.hpp              # event types and EventSequence
    screenshot.hpp            # pixel buffer, capture, PNG I/O
    snapshot-store.hpp        # golden read/write, directory layout
    image-diff.hpp            # pixel comparison, DiffResult, diff image
    visual-test-case.hpp      # VisualTestCase struct
    visual-test-registry.hpp  # parallel registry for visual tests
    visual-test-runner.hpp    # runner: update / verify / review modes
    visual-test-macros.hpp    # VISUAL_DESCRIBE, VISUAL_TEST, SEND, ASSERT_SNAPSHOT
    i-screen-capture.hpp      # platform abstraction for screenshot capture
    i-event-injector.hpp      # platform abstraction for input injection
  visual.hpp                  # single include for consumers

src/
  visual/
    snapshot-store.cpp
    image-diff.cpp
    visual-test-runner.cpp
    platform/
      x11-screen-capture.cpp
      win32-screen-capture.cpp
      macos-screen-capture.cpp
      x11-event-injector.cpp
      win32-event-injector.cpp
      macos-event-injector.cpp
```

---

## Event Model

### UIEvent

A discriminated union covering every input type the runner can inject:

```cpp
namespace Cimmerian::Visual {

struct MouseMoveEvent  { int x; int y; };
struct MouseClickEvent { int x; int y; int button; };  // button: 1=left 2=mid 3=right
struct MouseButtonPressEvent   { int x; int y; int button; };  // button: 1=left 2=mid 3=right
struct MouseButtonReleaseEvent { int button; };  // no x/y - targets whatever button is down
struct MouseScrollEvent{ int x; int y; int deltaX; int deltaY; };
struct KeyPressEvent   { int keyCode; };
struct KeyReleaseEvent { int keyCode; };
struct WaitEvent       { std::chrono::milliseconds duration; };

using UIEvent = std::variant<
    MouseMoveEvent,
    MouseClickEvent,
    MouseButtonPressEvent,
    MouseButtonReleaseEvent,
    MouseScrollEvent,
    KeyPressEvent,
    KeyReleaseEvent,
    WaitEvent
>;

} // namespace Cimmerian::Visual
```

### EventSequence

An ordered list of events. The runner dispatches them through the platform injector in sequence. `WaitEvent` causes the runner to sleep, giving the application time to render before the next event or snapshot is taken.

```cpp
struct EventSequence {
    std::vector<UIEvent> events;

    EventSequence& MouseMove(int x, int y);
    EventSequence& Click(int x, int y, int button = 1);
    EventSequence& MouseButtonPress(int x, int y, int button = 1);
    EventSequence& MouseButtonRelease(int button = 1);
    EventSequence& Scroll(int x, int y, int deltaX, int deltaY);
    EventSequence& KeyPress(int keyCode);
    EventSequence& KeyRelease(int keyCode);
    EventSequence& Wait(std::chrono::milliseconds ms);
};
```

---

## Screenshot

```cpp
struct Screenshot {
    std::vector<uint8_t> pixels;  // RGBA, row-major
    int width;
    int height;

    static Screenshot Capture(void* windowHandle);
    static Screenshot LoadPNG(const std::string& path);
    void SavePNG(const std::string& path) const;
};
```

`Capture` delegates to the platform-specific `IScreenCapture` implementation registered for the current build target. The window handle is whatever the platform uses (HWND, Window, NSWindow*, etc.) — passed as `void*` and cast internally.

### Platform Abstraction

```cpp
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;
    virtual Screenshot Capture(void* windowHandle) = 0;
};

class IEventInjector {
public:
    virtual ~IEventInjector() = default;
    virtual void Inject(const UIEvent& event) = 0;
};
```

The visual runner holds shared_ptrs to both. Platform implementations are registered at startup via a factory or passed in directly by the consumer. This keeps platform code out of the library core and lets consumers substitute a mock injector for unit-testing the runner itself.

---

## Snapshot Storage

Goldens live on disk under a configurable root directory (defaults to `./snapshots`). Each snapshot is named by its test path and an explicit label given at the assertion site.

```
snapshots/
  <group-name>/
    <test-name>/
      <snapshot-label>.golden.png
      <snapshot-label>.actual.png   ← written during verify, deleted on pass
      <snapshot-label>.diff.png     ← written only on failure
```

The store encodes group and test names as filesystem-safe slugs (spaces → hyphens, lowercased). The label is the string passed to `ASSERT_SNAPSHOT`.

```cpp
class SnapshotStore {
public:
    explicit SnapshotStore(const std::string& rootDir);

    std::string GoldenPath(
        const std::string& group,
        const std::string& test,
        const std::string& label) const;

    std::string ActualPath(...) const;
    std::string DiffPath(...) const;

    bool GoldenExists(
        const std::string& group,
        const std::string& test,
        const std::string& label) const;
};
```

---

## Image Comparison

```cpp
struct DiffResult {
    bool passed;
    int  differentPixelCount;
    float differencePercent;       // 0.0–100.0
    Screenshot diffImage;          // pixels highlighted where images differ
};

struct DiffOptions {
    float threshold = 0.1f;        // max allowed per-channel diff (0–1)
    float maxDifferencePercent = 0.0f;  // fail if more than this % of pixels differ
};

DiffResult CompareScreenshots(
    const Screenshot& golden,
    const Screenshot& actual,
    const DiffOptions& options = {});
```

The diff image uses a standard convention: matching pixels are rendered at reduced opacity, differing pixels are highlighted in red. This makes failures immediately obvious when opened in any image viewer.

Pixel comparison: for each pixel, compute the max absolute channel difference across R, G, B (ignoring alpha). If that value exceeds `threshold`, the pixel is counted as different. The test passes if `differencePercent <= maxDifferencePercent`.

---

## API / Macros

### VISUAL_DESCRIBE / VISUAL_TEST

Mirror the existing `DESCRIBE`/`TEST` API. `VISUAL_DESCRIBE` takes an extra parameter — the window handle — which is passed through to all snapshot assertions inside it.

```cpp
VISUAL_DESCRIBE("Settings Panel", settingsPanelWindow, {
    VISUAL_TEST("default state looks correct", {
        WAIT(50ms);
        ASSERT_SNAPSHOT("default-state");
    });

    VISUAL_TEST("hover on save button", {
        SEND(MouseMove(200, 340));
        WAIT(16ms);
        ASSERT_SNAPSHOT("save-button-hover");

        SEND(MouseClick(200, 340));
        WAIT(32ms);
        ASSERT_SNAPSHOT("save-button-clicked");
    });
});
```

### SEND

Injects a single event immediately:

```cpp
SEND(MouseMove(x, y));
SEND(MouseClick(x, y));
SEND(MouseClick(x, y, 3));   // right-click
SEND(KeyPress(KEY_ESCAPE));
SEND(MouseScroll(x, y, 0, -3));
```

### WAIT

Shorthand for injecting a `WaitEvent`:

```cpp
WAIT(16ms);   // one frame at 60fps
WAIT(100ms);
```

### ASSERT_SNAPSHOT

Captures the current window state and either writes it as the golden or compares it against the existing golden, depending on run mode:

```cpp
ASSERT_SNAPSHOT("label");
ASSERT_SNAPSHOT("label", DiffOptions{ .maxDifferencePercent = 0.5f });
```

On failure in verify mode, the actual and diff images are written to the snapshot directory and the test is failed with a message showing the diff percentage and paths.

An overload also accepts an explicit window handle, overriding the enclosing `VISUAL_DESCRIBE`'s (used by `VISUAL_DESCRIBE_COMPONENT`, below):

```cpp
ASSERT_SNAPSHOT("label", someWindowHandle);
ASSERT_SNAPSHOT("label", someWindowHandle, DiffOptions{ .maxDifferencePercent = 0.5f });
```

### NAVIGATE

Jumps the app under test straight to a named screen/route/state via a consumer-registered `NavigateFn`, bypassing `SEND`/`IEventInjector` entirely (`include/cimmerian/visual/navigation-driver.hpp`). This is for *reaching* a screen — real interaction tests still use `SEND`:

```cpp
// Once, in the consumer's test-main (or any static initializer) - bridges
// Cimmerian's opaque screenKey to the app's own router/view-model/IPC:
Cimmerian::Visual::ActiveNavigationDriver::GetInstance().Set([](const std::string& screenKey) {
    App::Router::NavigateTo(screenKey);
});

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

`NAVIGATE` throws if no `NavigateFn` has been registered — it is opt-in, not a silent no-op, since a `NAVIGATE` that quietly did nothing would let a following `ASSERT_SNAPSHOT` pass against whatever screen already happened to be showing.

### VISUAL_DESCRIBE_COMPONENT

A `VISUAL_DESCRIBE` group with no window handle of its own, for component-level tests where the window isn't known until each `VISUAL_TEST` mounts its own host. `MountComponent<T>` itself is consumer-supplied — Cimmerian only needs a `void*` window handle back, the same contract `IScreenCapture` already has:

```cpp
VISUAL_DESCRIBE_COMPONENT("SaveButton", {
    VISUAL_TEST("default", {
        auto host = MountComponent<SaveButton>(SaveButtonProps{ .label = "Save", .enabled = true });
        WAIT(16ms);
        ASSERT_SNAPSHOT("default", host.WindowHandle());
    });

    VISUAL_TEST("disabled", {
        auto host = MountComponent<SaveButton>(SaveButtonProps{ .label = "Save", .enabled = false });
        WAIT(16ms);
        ASSERT_SNAPSHOT("disabled", host.WindowHandle());
    });
});
```

For components whose visual states are themselves interaction-driven (hover, pressed, focus-ring), `SEND` against the host window remains appropriate — the host is dedicated and disposable, so the unscoped-global-input risk below has nothing else on screen to collide with.

`test/visual.test.cpp`'s `ScratchComponentHost`/`MountScratchComponent` stand in for a consumer's real `MountComponent<T>` — Cimmerian doesn't ship one, since the component/widget model is entirely consumer-specific (Amanuensis, pharos-proto, penumbra-proto would each need their own).

---

## Run Modes

The mode is set at the runner level, not per-test, so the whole suite runs in a single mode:

```cpp
enum class VisualRunMode {
    Verify,  // default: compare against goldens, fail on mismatch
    Update,  // write new goldens, always "pass"
    Review,  // verify + emit HTML report; does not fail the process
};
```

Controlled via a CLI flag or programmatically:

```cpp
// In test-main.cpp, alongside the standard runner:
Cimmerian::Visual::VisualTestRunner visualRunner;
visualRunner.SetMode(Cimmerian::Visual::VisualRunMode::Verify);
visualRunner.SetSnapshotRoot("./snapshots");
visualRunner.SetCapture(std::make_shared<X11ScreenCapture>());
visualRunner.SetInjector(std::make_shared<X11EventInjector>());

auto result = visualRunner.RunAll(
    Cimmerian::Visual::VisualTestRegistry::GetInstance()
);
```

Suggested CLI flags (parsed by the consumer's `main`):

```
--update-snapshots       runs in Update mode
--review-snapshots       runs in Review mode, writes report.html
--snapshot-dir <path>    override the golden directory (default: ./snapshots)
--filter <pattern>       only run visual tests whose name contains pattern
```

### Review Mode HTML Report

When `--review-snapshots` is passed, the runner writes `visual-report.html` to the snapshot root. Each test that has at least one snapshot gets a section showing:

- The test name and pass/fail status
- For each snapshot: golden | actual | diff, rendered side by side at actual resolution
- The diff percentage for each snapshot

This lets you scan the entire suite visually and decide whether to accept or reject changes before running `--update-snapshots`.

---

## Integration with the Existing TestRunner

Visual tests are registered in `VisualTestRegistry`, separate from the standard `TestRegistry`. The standard runner ignores them entirely. The entry point calls both runners in sequence:

```cpp
// test-main.cpp (consumer)
int main() {
    // Standard tests
    Cimmerian::TestRunner runner;
    auto summary = runner.RunAll(&Cimmerian::TestRegistry::GetInstance());
    PrintSummary(summary);

    // Visual tests (opt-in)
    Cimmerian::Visual::VisualTestRunner visualRunner;
    // ... configure ...
    auto visualSummary = visualRunner.RunAll(
        &Cimmerian::Visual::VisualTestRegistry::GetInstance()
    );
    PrintVisualSummary(visualSummary);

    return summary.failed + visualSummary.failed > 0 ? 1 : 0;
}
```

`VisualTestRunSummary` mirrors `TestRunSummary` and adds visual-specific fields:

```cpp
struct VisualTestRunSummary {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int updatedGoldens = 0;   // in Update mode
    int missingGoldens = 0;   // goldens that don't exist yet
    std::string reportPath;   // in Review mode
};
```

---

## Platform Considerations

| Capability       | Linux (X11)          | macOS              | Windows           |
|------------------|----------------------|--------------------|-------------------|
| Screenshot       | XGetImage / XShmGetImage | CGWindowListCreateImage | BitBlt / PrintWindow |
| Mouse inject     | XTestFakeMotionEvent | CGEventCreateMouseEvent | SendInput         |
| Click inject     | XTestFakeButtonEvent | CGEventCreateMouseEvent | SendInput         |
| Key inject       | XTestFakeKeyEvent    | CGEventCreateKeyboardEvent | SendInput      |
| Sleep            | std::this_thread::sleep_for (all platforms)             |

X11 injection requires `libXtst`. macOS requires the Accessibility permission (`AXIsProcessTrusted()`). Windows requires no extra permissions for `SendInput` to the same process.

CMake will gate the platform implementations behind `if(UNIX AND NOT APPLE)`, `if(APPLE)`, `if(WIN32)` blocks and expose a `CIMMERIAN_VISUAL_PLATFORM` option to let consumers override. On Linux, `Linux-auto` builds both `X11EventInjector` and `LinuxUinputEventInjector` and picks between them at `Probe()` time (`src/visual/platform/auto-linux-event-injector.cpp`) — it starts on the cheap XTEST path and only falls back to `/dev/uinput` if XTEST turns out to report success without actually reaching the compositor's input pipeline (see `docs/cimmerian_wayland_xtest_injection_gap.md`). A compositor that silently drops XTEST-over-XWayland input can't be made to forward it from the client side — that's inherent to that compositor's own XTEST support, not something fixable in `X11EventInjector` itself — so `Linux-auto` is the closest available thing to "make X11EventInjector reliable": pick the cheap backend by default, detect at runtime when it isn't working, and switch without the consumer having to know up front.

**X11 precondition on Wayland sessions:** X11 capture/injection can only see windows that exist in the X11 window tree. On a Wayland session (`XDG_SESSION_TYPE=wayland`), a window is only an X11/XWayland window if its toolkit explicitly opts into that — most Wayland-native toolkits default to their native Wayland backend instead, in which case the window never appears in the X11 tree at all, however visibly it's on screen. Common toolkits need an explicit env var set before launch to force X11/XWayland mode: `SDL_VIDEODRIVER=x11` (SDL3), `GDK_BACKEND=x11` (GTK4), `QT_QPA_PLATFORM=xcb` (Qt6). `X11ScreenCapture::Capture()` and the window-lookup helpers below warn and point at this when they can't find/attach to a window, but the env var itself is the actual fix.

**Both injection backends are real, global OS input, unscoped to the target window.** Neither XTEST/X11 nor the Linux kernel `/dev/uinput` layer (`LinuxUinputEventInjector`) has a "scoped to this window" concept the way e.g. a browser automation tool's isolated page context does - coordinates are screen-absolute, and if the target window loses real OS focus mid-test (another app's notification, an alt-tab, a background process raising a window), the next injected event goes wherever focus actually is, not necessarily the window under test. This is a real risk when running visual regression tests against a live, multi-window developer desktop rather than a dedicated `Xvfb`/CI display; `X11EventInjector` checks focus via `XGetInputFocus` before each `SEND()` and warns (without refusing) if the target window isn't focused, but this narrows rather than eliminates the race.

### Usage Guidance — which mechanism to reach for

The suite currently supports two ways to get a window under test:
building a scratch window inline (`test/visual.test.cpp`'s
`CreateTestWindow()`) or driving a real, separately-launched application
via `WaitForWindowByTitle`/`WaitForWindowByPid` below. In both cases,
getting the *window* on screen is separate from getting the right
*screen/state* within it on screen — the latter today only has one
mechanism, real platform input injection (`SEND`), which is also what
`ASSERT_SNAPSHOT`-adjacent interaction tests (hover, click, drag) use to
exercise the thing actually under test.

Guidance for which combination to use, now that `docs/
cimmerian_navigation_without_platform_input_proposal.md`'s alternatives
(`NAVIGATE`; `VISUAL_DESCRIBE_COMPONENT`) have landed:

- **Testing one component's appearance** (a button's default/hover/
  disabled states, a row's selected fill) — prefer `VISUAL_DESCRIBE_COMPONENT`
  with the smallest window that can render it, built inline the way
  `test/visual.test.cpp` does, not a full running app. Reserve `SEND` for
  the states that are genuinely interaction-driven; construct static
  states (disabled, error, empty) directly in whatever way the consumer's
  component API allows, without needing an event at all.
- **Testing a composed screen's layout** (does this panel look right with
  real sibling components, does this route render correctly) — a
  separately-launched real app is appropriate. Reach the screen with
  `NAVIGATE` rather than a `SEND` click-through-the-UI sequence: every
  `SEND` used purely to *navigate there* is a known reliability and safety
  risk, not a neutral setup step — it's real, global, unscoped OS input
  (`docs/cimmerian_live_app_visual_testing_gap.md` gaps 3-4), it can
  silently no-op on some Linux compositors unless `Probe()`/
  `IsFunctional()` is checked first (`docs/
  cimmerian_wayland_xtest_injection_gap.md`, `docs/
  cimmerian_uinput_no_functional_check_gap.md`), and a same-frame
  press/release can be missed entirely by once-per-frame input polling
  (`docs/cimmerian_mouse_click_no_hold_gap.md`). Keep `SEND` reserved for
  tests that are actually about an interaction, once already on the right
  screen.
- **Running against a live, multi-window desktop at all** (as opposed to
  a dedicated `Xvfb`/CI display) — expect occasional stray input if focus
  shifts mid-sequence, and don't run visual suites unattended on a
  machine someone is actively using. Prefer a dedicated display for CI.
- **Whichever mechanism is used**, always call `Probe()` (or let
  `VisualTestRunner::SetInjector()` do it) before trusting a "passing"
  golden that involved any `SEND` — a captured snapshot proves the
  screen looked a certain way, not that the input that got it there
  actually landed.

### Testing a real, separately-launched application

`WaitForWindowByTitle` / `WaitForWindowByPid` (`include/cimmerian/visual/window-lookup.hpp`, implemented per-platform under `src/visual/platform/`) poll for a window matching a title substring or owning pid, returning its handle as `void*` (or `nullptr` on timeout; on X11, with the Wayland-native-surface warning above if nothing ever matched). This is the realistic way to obtain a `VISUAL_DESCRIBE` window handle for an application spawned as its own subprocess, rather than a scratch window the test creates inline.

### PNG I/O

Use `stb_image` and `stb_image_write` (single-header, MIT). They are embedded directly into `src/visual/` as implementation files, so consumers have no extra dependencies to manage.

---

## Implementation Phases

### Phase 1 — Core infrastructure (no platform code)

- `UIEvent` variant and `EventSequence`
- `Screenshot` struct (pixel buffer only, no capture)
- `SnapshotStore` (path resolution, slug generation)
- `image-diff` (pixel comparison, diff image generation)
- `VisualTestCase`, `VisualTestRegistry`
- `VisualTestRunner` skeleton with Update/Verify modes
- PNG I/O via stb
- Unit tests for diff logic using synthetic screenshots

### Phase 2 — Platform capture and injection (one platform first)

- `IScreenCapture` and `IEventInjector` interfaces
- X11 implementation (the primary dev platform)
- Wire up `VisualTestRunner` to actually call capture + inject
- End-to-end test: a small SDL or GLFW window as the test subject

### Phase 3 — Macros and consumer API

- `VISUAL_DESCRIBE`, `VISUAL_TEST`, `SEND`, `WAIT`, `ASSERT_SNAPSHOT`
- Integrate with Amanuensis as the first real consumer

### Phase 4 — Review mode

- HTML report generation (single self-contained file, base64-embeds images)
- CLI flag parsing helpers

### Phase 5 — Remaining platforms

- macOS implementation
- Windows implementation
