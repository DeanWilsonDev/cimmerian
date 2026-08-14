# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-08-14.

## Read first

- **macOS capture-timing flakiness — fixed and live-verified (2026-08-14)** —
  root-caused in an earlier pass this same day (see the git history on this
  entry for the original `pharos-proto` repro/analysis) to
  `src/visual/platform/macos-screen-capture.mm`'s `MacOSScreenCapture::Capture`
  reading `targetWindow.frame` once from a single `SCShareableContent`
  snapshot, before the async `SCScreenshotManager` capture completes, with no
  re-check — a stale-*size* race, distinct from the X11 stale-*pixel-content*
  race below.

  **Fixed by extracting `ResolveStableWindowTarget`**, which re-fetches
  `SCShareableContent` and re-reads `targetWindow.frame` in a loop (up to 8x,
  16ms apart) until two consecutive reads agree, only then handing that frame
  to `config.width`/`config.height` — the same "agreement is the real signal"
  shape as the X11 poll below, but comparing frame *size* via
  `CGRectEqualToRect` instead of pixel content, since ScreenCaptureKit already
  reads composited pixels synchronously (no compositor-flip race to poll for
  there). Logs a warning and uses the last-read frame if it never stabilizes,
  same fallback shape as X11. The whole-display capture path (`windowHandle ==
  nullptr`) is untouched — a display's frame doesn't race like a
  window-in-motion does.

  **Build- and live-verified this session** (unlike the X11 fix below, this
  ran on the Darwin machine that has the real toolchain): `cmake
  -DCIMMERIAN_BUILD_TESTS=ON` + `cmake --build .` compiles clean (only a
  pre-existing, unrelated enum-mixing warning on the same file), and
  `test_cimmerian` run live 15x back-to-back against real on-screen scratch
  windows (`Scratch Window macOS`, `Scratch Component macOS`) — all 15 runs
  passed with 0 snapshot diffs. Not a guarantee the race can never happen
  (the repro was intermittent, ~1-2 of 6 tests, in a different codebase's
  suite), but a real compile + live-run pass, not just code review.

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
- **Win32 window-lookup still isn't compile-tested** (carried over — no
  Windows toolchain available here). macOS window-lookup/event-injector are
  now compile- and live-tested as of this session (see "What changed this
  session"): this machine has a real Darwin toolchain and Screen Recording
  permission already granted, which the earlier "no macOS toolchain
  available" note assumed wasn't the case.

## What changed this session

Fixed the macOS capture-timing flakiness documented above:

- `src/visual/platform/macos-screen-capture.mm`: extracted
  `ResolveStableWindowTarget`, which polls (up to 8x, 16ms apart) re-fetching
  `SCShareableContent` and re-reading the target window's `frame` until two
  consecutive reads agree via `CGRectEqualToRect`, before committing to
  `config.width`/`config.height`. `MacOSScreenCapture::Capture`'s window path
  now calls this instead of reading `frame` once; the display-capture path is
  unchanged.
- Verified with a real build + live run on this Darwin machine: configured a
  separate `-DCIMMERIAN_BUILD_TESTS=ON` build tree, compiled clean, and ran
  `test_cimmerian` 15x back-to-back against real on-screen scratch windows —
  0 failures, 0 snapshot diffs across all runs.
