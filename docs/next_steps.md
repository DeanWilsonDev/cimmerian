# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-08-14.

## Read first

- **macOS capture-timing flakiness — partial fix only, re-verified against
  `pharos-proto` and still flaky (2026-08-14, follow-up pass same day).**
  Original root cause (see git history on this entry for the full original
  analysis): `src/visual/platform/macos-screen-capture.mm`'s
  `MacOSScreenCapture::Capture` read `targetWindow.frame` once from a single
  `SCShareableContent` snapshot, before the async `SCScreenshotManager`
  capture completed, with no re-check — a stale-*size* race, distinct from
  the X11 stale-*pixel-content* race below.

  **First fix (this same day, see "What changed" further down):** extracted
  `ResolveStableWindowTarget`, polling `SCShareableContent`/`targetWindow.frame`
  (up to 8x, 16ms apart) until two consecutive reads agree via
  `CGRectEqualToRect`, before committing to `config.width`/`config.height`.
  Verified at the time with 15 back-to-back live runs against this repo's own
  scratch windows, 0 failures — but that verification never re-ran the actual
  original repro (`pharos-proto`'s `pharos_visual_tests`), just this repo's
  own, differently-shaped scratch-window tests.

  **Re-verified directly against the original repro and the flakiness is
  still there, just narrower.** `pharos-proto` bumped its `cimmerian` pin to
  this fix (`bde855b`), re-captured its goldens against the fixed code (so
  the comparison is fair — not stale goldens), then ran
  `pharos_visual_tests` 20x back-to-back: only 6/20 runs were fully clean.
  The failure shape changed from before the fix (previously both width and
  height could be off by ~4px) to **height-only, a discrete two-state
  value** — e.g. one specific test (`Inspector Panel (component) > data
  loaded with no selection`) came back consistently at *either* 1464 or 1468
  px tall, never in between, across many repeated captures of the same
  static (non-animating) window state. Width was always correct in every
  observed failure.

  **Root cause of the remaining gap, not yet fixed:** `TEST_LOG_WARN`'s
  "did not stabilize" message (the fallback path when the poll never finds
  two agreeing reads) never fired in any of the ~30 combined verification
  runs across both sessions — confirmed by grepping test output. That means
  `ResolveStableWindowTarget` *is* finding two consecutive agreeing reads
  every time; the two-state jitter must be happening in the gap the fix
  doesn't cover: between `ResolveStableWindowTarget`'s last confirming read
  and the actual `CaptureImageSync(filter, config)` call
  (`macos-screen-capture.mm`, `Capture`'s body) — nothing re-confirms the
  frame is still the same size at the moment the real capture request goes
  out. The fix narrowed the race's window and reduced its typical magnitude
  (was ~4px on either axis, now ~4px on height only), it didn't close it.

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

- **macOS capture-timing flakiness is still open** (see "Read first" above):
  `ResolveStableWindowTarget`'s poll only confirms the frame is stable up to
  the moment of its *last read* — there's still an unguarded gap between
  that read and the actual `CaptureImageSync` call in `Capture` where the
  window can change again. A real fix likely needs one of: re-reading
  `targetWindow.frame` a final time immediately before (or after)
  `CaptureImageSync` and retrying the whole capture if it changed, or
  polling on the *returned image's* actual `CGImageGetWidth`/`GetHeight`
  against the expected size instead of only trusting the pre-capture frame
  read. Not attempted here — flagging the gap, not guessing at the shape
  under time pressure the way the first pass's fix arguably did.
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
