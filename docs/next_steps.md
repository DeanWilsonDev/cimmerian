# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-07-21.

## Read first

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
