# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-07-21.

## Read first

- `docs/visual-regression-spec.md` / `docs/snapshot-testing-spec.md` — the
  extension's own design docs.
- `docs/cimmerian_wayland_xtest_injection_gap.md` — why
  `LinuxUinputEventInjector` exists at all (XTEST-over-XWayland silently
  no-ops on at least one real Wayland compositor).
- `docs/cimmerian_mouse_click_no_hold_gap.md` — implemented this session,
  see below.

## What's actually left open

- **`X11EventInjector`'s XTEST-over-XWayland gap (`cimmerian_wayland_xtest_injection_gap.md`)**
  — `LinuxUinputEventInjector` is the working fallback and is what's
  actually in use; the underlying "XTEST reports success but does
  nothing" issue on some compositors was never independently fixed,
  routed around instead. Still open if anyone wants `X11EventInjector`
  itself to be reliable rather than just avoided.
- **Uncommitted**: the `MouseButtonPressEvent`/`MouseButtonReleaseEvent`
  implementation below is in the working tree, not yet committed. Review
  and commit before anything downstream (e.g. `pharos-proto`) tries to
  vendor this checkout.

## What changed this session

Implemented `docs/cimmerian_mouse_click_no_hold_gap.md` §3 in full:
`MouseButtonPressEvent`/`MouseButtonReleaseEvent` added to
`include/cimmerian/visual/ui-event.hpp` (alongside the unchanged
`MouseClickEvent`), `EventSequence::MouseButtonPress`/`MouseButtonRelease`
added, all four `IEventInjector` backends
(`linux-uinput-event-injector.cpp`, `x11-event-injector.cpp`,
`win32-event-injector.cpp`, `macos-event-injector.cpp`) gained matching
press/release branches, `visual-test-macros.hpp` gained the `SEND()` shim
functions, and `docs/visual-regression-spec.md`'s event-type table was
updated to match. Built and ran the existing test suite on Linux
(`X11EventInjector`/`LinuxUinputEventInjector` paths, the two compiled on
this platform) — clean build, no regressions (the one failing test,
`example.test.cpp`'s "Compare Vectors", is a pre-existing intentional
demo of the diff formatter, unrelated to this change). `Win32EventInjector`/
`MacOSEventInjector` weren't compile-tested (no Windows/macOS toolchain
here) but mirror the exact shape of their own existing `MouseClickEvent`
branches.

Filed `pharos-proto/docs/pharos_click_hold_selection_test_feature_request.md`
as a `/feature-request` — re-attempting the abandoned Explorer-row-selection
/ `ColorFilterDropdown` click test flagged in that repo's own
`pharos_toolbar.visual.test.cpp` trailing comment is Pharos's own work to
pick up now that this primitive exists, not something to implement here.
