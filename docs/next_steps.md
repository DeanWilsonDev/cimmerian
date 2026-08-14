# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-08-14.

## Read first

- **macOS capture-timing flakiness — both fixes stay, but neither was ever
  this repo's bug. Root-caused in `pharos-proto` to that repo's own test
  harness, not `cimmerian`'s capture code (2026-08-14, third pass same
  day).** Re-verified `c846354` directly against the original `pharos-proto`
  repro (20-30 repeated runs against freshly-recaptured goldens, matching the
  first fix's own verification methodology): no improvement (0/16 clean in
  one batch) and real added latency from the retry loop. Crucially, neither
  fix's `TEST_LOG_WARN` fallback ever fired across ~60 combined verification
  runs on the `pharos-proto` side — both fixes' own internal consistency
  checks (frame-reads-agree; then captured-size-matches-requested-size)
  passed every single time. That's conclusive: the window's real on-screen
  geometry is already fixed *before* `MacOSScreenCapture::Capture` ever runs,
  so nothing at this layer — however much polling or retrying — could ever
  have closed this gap. It was never a capture-code race.

  **Real root cause, found in `pharos-proto`'s own test harness:** confirmed
  directly against the test machine's screen (`NSScreen.main.frame` =
  `(0, 0, 1512, 982)` pts, `backingScaleFactor` `2.0` — a clean scale, not a
  fractional-rounding issue). `pharos-proto`'s `pharos_in_process_host.cpp`
  requests a `1600×900`-point test window, which is *wider than the screen
  itself* (1512pt) — macOS has to clamp/negotiate that to fit, and the
  negotiated result varies by ~2pt on both axes between separate window
  creations. A second, differently-shaped case in the same repo
  (`inspector_component_host.cpp`, a 360×700 window that fits the screen
  easily) still jitters ~2pt on height alone, consistent with
  `SCShareableContent`'s window frame including title-bar chrome with a
  couple points of finalization jitter. Neither is anything `cimmerian` can
  see or fix from inside `Capture` — both originate in how the target
  application's own window gets created and negotiated with the window
  server, before any capture call happens. Full trace, with exact point
  numbers and file:line citations, is in `pharos-proto/docs/next_steps.md`'s
  "Visual-test capture-timing flakiness" entry.

  **Neither `bde855b` nor `c846354` is being reverted.** Both are real,
  independently-useful fixes for a genuine (if rarer, or simply not what
  triggered this specific repro) mid-resize capture race — `c846354`'s own
  check never producing a false rejection across every verification run here
  is itself evidence it isn't introducing bad behavior, only unnecessary
  latency on captures that were already fine. They're just not the fix for
  *this* bug, since this bug's cause sits upstream of anything this repo's
  capture code can observe.

  Original root cause this whole thread started from (see git history on this
  entry for the full original analysis): `MacOSScreenCapture::Capture` read
  `targetWindow.frame` once from a single `SCShareableContent` snapshot,
  before the async `SCScreenshotManager` capture completed, with no
  re-check — a stale-*size* race, distinct from the X11 stale-*pixel-content*
  race below.

  **First fix** extracted `ResolveStableWindowTarget`, polling
  `SCShareableContent`/`targetWindow.frame` (up to 8x, 16ms apart) until two
  consecutive reads agree via `CGRectEqualToRect`. Verified at the time
  against this repo's own scratch windows only (15 clean runs) — re-verified
  directly against the original repro (`pharos-proto` bumped its pin to
  `bde855b`, re-captured goldens against the fixed code, ran
  `pharos_visual_tests` 20x): only 6/20 clean. Root-caused the remainder to a
  gap the first fix didn't cover — `ResolveStableWindowTarget`'s own "did not
  stabilize" warning never fired in ~30 combined runs (it kept finding two
  agreeing *pre-capture* reads), yet captures still came back height-off by a
  discrete ~4px — meaning the window could still change *after* the poll's
  last confirming read but *before* `CaptureImageSync`'s completion handler
  actually fired, and nothing re-confirmed the frame at that point.

  **Second fix (this pass): closes that gap by checking ground truth instead
  of another indirect frame read.** Extracted `CaptureWindowStable`, which
  wraps the *entire* resolve+capture cycle (not just the pre-capture poll) in
  a retry loop (up to 4 attempts): resolve a stable frame, build the capture
  config from it, capture, then compare the *actual* `CGImageGetWidth`/
  `GetHeight` of the returned image against the `config.width`/`height` that
  frame predicted. A mismatch means the window changed in the gap the first
  fix couldn't see — retry the whole cycle rather than trusting the
  pre-capture read further. Falls back to returning the last capture with a
  `TEST_LOG_WARN` if all 4 attempts still mismatch, same "don't hang, warn
  and return something" shape as both earlier fixes. `MakeConfig` was
  extracted as a shared helper (window and display paths both use it now)
  with no behavior change to the display path.

  **Verified at the time by build + 20 live runs against this repo's own
  scratch windows — 0 failures, 0 "did not stabilize"/"still didn't match"
  warnings** — but, same caveat as the first fix's initial verification,
  this repo's static scratch windows have never reproduced the original
  race themselves; only `pharos-proto`'s suite has. **Re-verified against
  `pharos-proto` in the third pass above, and it didn't close the gap** —
  see the top of this entry for why (the real cause was never in this
  repo's capture code).

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

- **macOS capture-timing flakiness — closed on this repo's side** (see "Read
  first" above): re-verified against the original `pharos-proto` repro and
  root-caused to that repo's own test harness (an oversized test window, and
  a title-bar-inclusive frame measurement), not `cimmerian`. Nothing left to
  do here unless `pharos-proto`'s own fix (tracked in its own
  `docs/next_steps.md`) surfaces a genuine capture-code gap once applied —
  not expected, but worth a glance if it comes back.
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

**Third pass, same day — no code change, a re-verification + root-cause
correction.** Prior passes this same day landed two capture-side fixes
(`bde855b`, `c846354`, see "Read first" above for both). This pass
re-verified `c846354` directly against the original `pharos-proto` repro
(the same one that caught `bde855b`'s own gap) instead of only this repo's
own scratch windows, and found it didn't help — then traced the actual cause
to `pharos-proto`'s own test harness (an oversized test window relative to
the real screen, plus a title-bar-inclusive frame measurement in a second
test), not anything in this repo's capture code. Both fixes are being kept
(real, independently-useful improvements against a genuine but different
race) — see "Read first" for the full trace and reasoning, and
`pharos-proto/docs/next_steps.md`'s own entry for the fix that actually
needs to land, in that repo.

Earlier this same day: closed the gap `bde855b` left open (see "Read first"
for why that fix wasn't enough) by extracting `CaptureWindowStable` in
`src/visual/platform/macos-screen-capture.mm`, which wraps the whole
resolve-frame + build-config + capture cycle in a retry loop (up to 4
attempts) instead of trusting `ResolveStableWindowTarget`'s pre-capture poll
alone — after each capture it compares the returned image's actual
`CGImageGetWidth`/`GetHeight` against the `config.width`/`height` the
resolved frame predicted, retrying the whole cycle on mismatch rather than
just re-polling the frame, and falling back to the last capture with a
`TEST_LOG_WARN` if all attempts still mismatch. Also extracted `MakeConfig`
as a shared helper used by both the window and display capture paths (no
behavior change to the display path). Verified at the time with a real
build + 20 live runs against this repo's own scratch windows — 0 failures,
0 warnings — which this pass's re-verification against `pharos-proto` shows
wasn't sufficient to prove the actual race was closed, since this repo's own
scratch windows never reproduced it in the first place.
