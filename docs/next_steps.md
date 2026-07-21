# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-07-21.

## Read first

- `docs/visual-regression-spec.md` / `docs/snapshot-testing-spec.md` — the
  extension's own design docs.
- `docs/cimmerian_wayland_xtest_injection_gap.md` — addressed this
  session, see below.
- `docs/cimmerian_live_app_visual_testing_gap.md` — implemented last
  session, extended this session (Win32/macOS window lookup).

## What's actually left open

- Nothing tracked as blocking. The two items flagged at the end of the
  last session (XTEST-over-XWayland reliability, Win32/macOS window
  lookup) are both done — see below.
- **`Linux-auto`'s `AutoLinuxEventInjector` is untested on a compositor
  where XTEST actually works** (this session's dev environment has
  XTEST-over-XWayland non-functional, so only the fallback path was
  exercised live) — the non-fallback path is just `X11EventInjector`
  unchanged, so risk is low, but worth knowing if something looks off on
  a compositor that forwards XTEST correctly.
- **Win32/macOS window-lookup and `AutoLinuxEventInjector` weren't
  compile-tested** (no Windows/macOS toolchain, and this Linux
  environment's XTEST is non-functional so uinput was exercised but not
  a working-XTEST codepath) — they mirror the exact shape of this
  codebase's existing platform backends, but flagging in case something
  doesn't build cleanly on a real Windows/macOS toolchain.

## What changed this session

Addressed both items flagged as open at the end of the previous session:

**1. `cimmerian_wayland_xtest_injection_gap.md`'s underlying issue.** A
compositor that silently drops XTEST-over-XWayland input can't be fixed
from the X11 client side — that's the compositor's own choice, not a
Cimmerian bug — so instead added `AutoLinuxEventInjector`
(`CIMMERIAN_VISUAL_PLATFORM=Linux-auto`,
`src/visual/platform/auto-linux-event-injector.cpp`): starts on the cheap
`X11EventInjector`/XTEST path, and falls back to
`LinuxUinputEventInjector` automatically if `Probe()` finds XTEST
non-functional on the current compositor. Wired into
`test/test-main.cpp` and `include/cimmerian/visual.hpp`. Verified live in
this session's own KDE/XWayland environment: `Probe()` correctly detects
XTEST is non-functional here and falls back to uinput, which then reports
functional.

**2. Win32/macOS `WaitForWindowByTitle`/`WaitForWindowByPid`.**
Refactored the X11-only version from last session into a single shared
public header (`include/cimmerian/visual/window-lookup.hpp`, moved from
`include/cimmerian/visual/platform/x11-window-lookup.hpp`) with
per-platform implementations: `src/visual/platform/win32-window-lookup.cpp`
(`EnumWindows`/`GetWindowThreadProcessId`) and
`src/visual/platform/macos-window-lookup.cpp`
(`CGWindowListCopyWindowInfo`), alongside the existing X11 one. Wired into
CMake for all five platform values.

Built and ran the full test suite for all three Linux
`CIMMERIAN_VISUAL_PLATFORM` values (`X11`, `Linux-uinput`, `Linux-auto`) —
clean builds, no regressions (the one failing test, `example.test.cpp`'s
"Compare Vectors", is the same pre-existing intentional demo of the diff
formatter noted in prior sessions, unrelated to this work).
