# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-08-14.

## Read first

- **macOS capture-timing flakiness — a new, different bug from the X11 one below
  (2026-08-14)** — found in `pharos-proto` while verifying that repo's own visual
  regression suite after replacing its stale Linux goldens with freshly-captured
  macOS ones: `pharos_visual_tests` intermittently reports "100% different" on
  1-2 of 6 tests, with `.actual.png` a few pixels smaller than its golden in
  width and/or height (e.g. 708×1440 vs. golden 712×1444) — confirmed by `sips`
  on several failing captures. Confirmed non-systemic, not a bad golden: an
  immediate rerun of the same test passes clean with no snapshot changes.

  **Root cause traced to `src/visual/platform/macos-screen-capture.mm`'s
  `MacOSScreenCapture::Capture`, not yet fixed.** `frame = targetWindow.frame`
  (line 96) reads the target window's size once, from the `SCShareableContent`
  snapshot `FetchShareableContentSync()` enumerates at the *start* of `Capture`
  (line 78) — `config.width`/`config.height` (lines 117-118) are computed from
  that one `frame` value and handed to `SCScreenshotManager
  captureImageWithFilter:configuration:` (line 49) as a fixed target size.
  There's no re-check that the window's actual size still matches `frame` by
  the time the asynchronous capture completes, and no retry/stability loop at
  this layer or its only caller (`Screenshot::Capture`,
  `src/visual/screenshot.cpp:9-18`, a bare pass-through with no settle logic of
  its own). If the target window is still resizing/settling (e.g. right after
  window creation or a state transition in the test) when
  `FetchShareableContentSync()` enumerates it, `targetWindow.frame` is stale by
  a few points relative to the window's true size at actual-capture time — the
  captured image comes back a few pixels off, exactly the symptom observed.

  **This is a different bug from the X11 flakiness below, not the same one on
  a new platform.** The X11 fix (`fc33eb0`) explicitly assumed macOS capture
  "doesn't have this problem (it reads already-composited output
  synchronously)" and left `macos-screen-capture.mm` untouched — that
  assumption covered *stale pixel content at a fixed, correct size* (the X11
  failure mode: compositor hadn't flipped the presented buffer), but not *the
  window's size itself being stale* at the moment of the size-determining
  `frame` read, which this finding shows macOS capture can still race. A fix
  here would need a different shape than X11's "poll until two captures agree"
  (that assumes a fixed frame size and compares pixel content) — more likely,
  re-reading `targetWindow.frame`/re-fetching shareable content immediately
  before `CaptureImageSync`, or polling until two consecutive `frame` reads
  agree, before committing to a `config.width`/`config.height`. Not
  implemented here — flagging the gap and its root cause, not fixing it.

- **X11 capture-timing flakiness (2026-08-14)** — traced from
  `pharos-proto/docs/next_steps.md`'s "Visual-test capture-timing
  flakiness" open item, which suspected a compositor/X11 vsync race in
  this repo's capture path (`XGetImage` reading before the target app's
  presented buffer is actually flipped) rather than an application bug.
  Confirmed: `src/visual/platform/x11-screen-capture.cpp`'s `Capture` did
  a single unsynchronized `XGetImage` with no wait for the compositor to
  settle — `XSync` on this process's own connection can't help, since the
  target app renders/presents on a *different* X connection and GPU
  timeline. Fixed by polling: capture repeatedly (up to 8x, 16ms apart)
  until two consecutive captures are pixel-identical, treating agreement
  as the real "stopped changing" signal instead of a fixed guessed delay;
  logs a warning and returns the last frame if it never stabilizes rather
  than hanging. macOS's `ScreenCaptureKit`-based capture doesn't have this
  problem (it reads already-composited output synchronously), so only
  `x11-screen-capture.cpp` changed. **Not yet build- or live-verified**:
  this session ran on a Darwin machine with no X11 headers/Xvfb
  available, so `CIMMERIAN_VISUAL_PLATFORM` defaults to `macOS` and
  `x11-screen-capture.cpp` isn't even part of this build here — reviewed
  carefully by hand instead. Needs a real compile + a live run against an
  actual X11/compositor session (this repo's own visual tests, or
  pharos-proto's) before trusting it fixes the flakiness pharos-proto
  measured.

- `docs/cimmerian_navigation_without_platform_input_proposal.md` —
  implemented this session: `NAVIGATE`/`ActiveNavigationDriver` (Proposal
  A) and `VISUAL_DESCRIBE_COMPONENT`/window-handle-overriding
  `ASSERT_SNAPSHOT` (Proposal B's Cimmerian-owned half). See doc header
  for exact mapping.
- `docs/visual-regression-spec.md` — Usage Guidance section updated to
  point at `NAVIGATE`/`VISUAL_DESCRIBE_COMPONENT` as the now-preferred
  mechanisms instead of describing them as still-pending.
- `docs/snapshot-testing-spec.md` — unrelated extension's own design doc.
- `docs/cimmerian_uinput_no_functional_check_gap.md` / `docs/
  cimmerian_wayland_xtest_injection_gap.md` / `docs/
  cimmerian_live_app_visual_testing_gap.md` / `docs/
  cimmerian_mouse_click_no_hold_gap.md` — addressed in prior sessions.

## What's actually left open

- **macOS capture-timing flakiness needs an actual fix** (see "Read first"
  above): `MacOSScreenCapture::Capture` reads the target window's `frame`
  once, before the async `SCScreenshotManager` capture completes, with no
  re-check or retry — root-caused, not yet fixed. A different shape than the
  X11 stability-poll fix, since this is the *size* going stale, not pixel
  *content* at a fixed size.
- **X11 capture-stability-polling fix needs real verification** (see "Read
  first" above): compile + run this repo's own `pharos_visual_tests`-style
  suite (or `test_cimmerian`'s visual tests) against a real X11/compositor
  session, ideally the same machine/setup that measured pharos-proto's
  ~77%→~90% pass-rate-with-3-PumpFrame baseline, to confirm the flakiness
  is actually gone rather than just plausible from code review.
- **`MountComponent<T>`'s exact API and `NavigateFn`'s out-of-process
  transport** were explicitly left for a design pass with a real consumer
  (Amanuensis, pharos-proto, penumbra-proto) — `test/visual.test.cpp`'s
  `ScratchComponentHost`/`NavigateScratchWindow` are illustrative
  in-process stand-ins only, not a reusable implementation.
- **`Linux-auto`'s `AutoLinuxEventInjector` is untested on a compositor
  where XTEST actually works** (carried over — this dev environment's
  XTEST-over-XWayland is still non-functional, so only the fallback path
  has ever been exercised live).
- **Win32/macOS window-lookup and `AutoLinuxEventInjector` still aren't
  compile-tested** (carried over — no Windows/macOS toolchain available
  here).

## What changed this session

Implemented `docs/cimmerian_navigation_without_platform_input_proposal.md`:

- Added `include/cimmerian/visual/navigation-driver.hpp` +
  `src/visual/navigation-driver.cpp` (`NavigateFn`, `ActiveNavigationDriver`)
  and the `NAVIGATE(screenKey)` macro in `visual-test-macros.hpp`.
  Throws (doesn't silently no-op) if no `NavigateFn` is registered.
- Added `VISUAL_DESCRIBE_COMPONENT` macro (a `VISUAL_DESCRIBE` with no
  group-level window handle) and a `void*`-window-handle-overriding
  `ASSERT_SNAPSHOT`/`VisualTestRunner::AssertSnapshot` overload, so
  component-host tests can hand their own mounted window's handle
  straight to the snapshot assertion instead of relying on a
  `VISUAL_DESCRIBE`-level one.
- Wired both into `include/cimmerian/visual.hpp` and
  `CMakeLists.txt` (`src/visual/navigation-driver.cpp`).
- Added demo coverage in `test/visual.test.cpp`: `NavigateScratchWindow`
  (registers a `NavigateFn` against the existing scratch X11 window,
  recoloring it per `screenKey`) and `ScratchComponentHost`/
  `MountScratchComponent` (a per-test disposable host window,
  standing in for a consumer's `MountComponent<T>`). Both new visual
  tests pass in Update and Verify mode against a real X11 display
  (`DISPLAY=:0` — no Xvfb available in this environment).
- Updated `docs/visual-regression-spec.md` (new `NAVIGATE`/
  `VISUAL_DESCRIBE_COMPONENT`/`ASSERT_SNAPSHOT`-overload sections, Usage
  Guidance rewritten to point at them) and the proposal doc's own status
  line to "implemented".
