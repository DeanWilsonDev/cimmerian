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
- `docs/cimmerian_live_app_visual_testing_gap.md` — implemented this
  session, see below.

## What's actually left open

- **`X11EventInjector`'s XTEST-over-XWayland gap (`cimmerian_wayland_xtest_injection_gap.md`)**
  — `LinuxUinputEventInjector` is the working fallback and is what's
  actually in use; the underlying "XTEST reports success but does
  nothing" issue on some compositors was never independently fixed,
  routed around instead. Still open if anyone wants `X11EventInjector`
  itself to be reliable rather than just avoided.
- **Win32/macOS `WaitForWindow*` equivalents** — this session's window-lookup
  fix (`x11-window-lookup.cpp`) only covers Linux (X11 window-tree
  polling). No `HWND`/`NSWindow*` equivalent exists yet; add one if a
  Windows/macOS consumer needs the same "launch a separate app, find its
  window" pattern.
- **`penumbra-proto`'s visual regression tests** — were on hold pending
  this session's fixes; unblocked now, but re-attempting them is
  `penumbra-proto`'s own work, not tracked here.

## What changed this session

Implemented `docs/cimmerian_live_app_visual_testing_gap.md` in full (all
four gaps found wiring visual-regression tests up against a real,
separately-launched app rather than a scratch test window):

- Added `WaitForWindowByTitle`/`WaitForWindowByPid`
  (`include/cimmerian/visual/platform/x11-window-lookup.hpp`,
  `src/visual/platform/x11-window-lookup.cpp`) — polls the X11 window tree
  by title substring or owning pid, built for both the X11 and
  Linux-uinput backends.
- `X11EventInjector`'s real-cursor `XQueryPointer` probe is no longer an
  unconditional constructor side effect — `IEventInjector` gained a
  `Probe()` virtual, `VisualTestRunner::SetInjector()` calls it once,
  deliberately.
- `IEventInjector` gained `IsWindowFocused()`; `X11EventInjector`
  implements it via `XGetInputFocus`, and `VisualTestRunner::SendEvent()`
  warns (without refusing) when the target window isn't focused before
  injecting.
- `docs/visual-regression-spec.md` now documents the X11/Wayland-native-
  surface precondition and that both Linux backends are unscoped global
  OS input; `X11ScreenCapture::Capture()` warns with the same explanation
  when window lookup fails.

Built and ran the existing test suite on Linux (`X11EventInjector`/
`LinuxUinputEventInjector` paths) — clean build, no regressions (the one
failing test, `example.test.cpp`'s "Compare Vectors", is a pre-existing
intentional demo of the diff formatter, confirmed present on `main`
before this session's changes). Committed as `39dcad7`.
