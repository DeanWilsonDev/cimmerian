# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-07-21.

## Read first

- `docs/visual-regression-spec.md` / `docs/snapshot-testing-spec.md` — the
  extension's own design docs.
- `docs/cimmerian_uinput_no_functional_check_gap.md` — filed and
  addressed this session, see below.
- `docs/cimmerian_wayland_xtest_injection_gap.md` / `docs/
  cimmerian_live_app_visual_testing_gap.md` — addressed in prior sessions.

## What's actually left open

- **`Linux-auto`'s `AutoLinuxEventInjector` is untested on a compositor
  where XTEST actually works** (carried over from last session — this
  dev environment's XTEST-over-XWayland is still non-functional, so only
  the fallback path has ever been exercised live).
- **Win32/macOS window-lookup and `AutoLinuxEventInjector` still aren't
  compile-tested** (carried over — no Windows/macOS toolchain available
  here).

## What changed this session

Investigated a `pharos-proto` report: a `CIMMERIAN_VISUAL_PLATFORM=
Linux-uinput` visual test (a held-press click, `MouseButtonPress`/`Wait`/
`MouseButtonRelease`) ran clean, captured a "passing" golden, but the
click never actually registered in the app under test. Traced with a
standalone program (independent of both Cimmerian and pharos-proto,
reproducing `LinuxUinputEventInjector`'s own ioctl sequence by hand): the
`/dev/uinput` device is created successfully (visible in `/proc/bus/
input/devices`, real evdev handler nodes, no ioctl failures) but produces
zero pointer movement in that particular sandboxed session — confirmed
both with cimmerian's own `ABS`+`INPUT_PROP_DIRECT` device shape and a
plain `REL_X`/`REL_Y` mouse device, ruling out a device-classification
explanation. `xdotool getmouselocation` (an X11 *read*, not an injection
path) confirms the position never changes. Wrote up
`docs/cimmerian_uinput_no_functional_check_gap.md` with the full repro,
root-cause analysis (same "protocol call succeeds, compositor doesn't
actually route it" shape as the original XTEST gap, just never given the
same self-check treatment), and a concrete proposed `Probe()`
implementation, then implemented it: `LinuxUinputEventInjector` now
overrides `IsFunctional()`/`Probe()` with a before/after `XQueryPointer`
round-trip around a real `MoveTo()` call (mirroring
`X11EventInjector::ProbeInjection()`), restoring the original pointer
position and logging a `TEST_LOG_WARN` when the move doesn't land.
`AutoLinuxEventInjector::Probe()` was also fixed to actually call the
uinput fallback's `Probe()` (it previously just read `IsFunctional()`,
which nothing had ever set to `false`). Verified live in this session's
own sandboxed shell: the probe correctly detects uinput events aren't
reaching the real pointer here and logs the warning instead of a silent
false pass. Built and ran the full test suite for all three Linux
`CIMMERIAN_VISUAL_PLATFORM` values (`X11`, `Linux-uinput`, `Linux-auto`) —
clean builds, no regressions (the one failing test, `example.test.cpp`'s
"Compare Vectors", is the same pre-existing intentional demo of the diff
formatter noted in prior sessions, unrelated to this work).
