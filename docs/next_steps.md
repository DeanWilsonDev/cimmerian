# Cimmerian — Next Steps

> A handoff pointer, not a spec — kept short deliberately. Refreshed at
> the end of each work session; supersedes its own previous contents
> rather than accumulating history (the individual gap/spec docs are the
> durable record).
> Last updated: 2026-07-20.

## Read first

- `docs/visual-regression-spec.md` / `docs/snapshot-testing-spec.md` — the
  extension's own design docs.
- `docs/cimmerian_wayland_xtest_injection_gap.md` — why
  `LinuxUinputEventInjector` exists at all (XTEST-over-XWayland silently
  no-ops on at least one real Wayland compositor).
- `docs/cimmerian_mouse_click_no_hold_gap.md` — new this session, see
  below.

## What's actually left open

- **`MouseClickEvent` has no held-press primitive.** Every
  `IEventInjector` backend (`Linux-uinput`, `X11`, `Win32`, `macOS`)
  presses and releases a mouse button with no gap in between. Target
  applications that poll input state once per frame rather than draining
  an event queue (confirmed concretely against `penumbra-proto`'s
  `PlatformWindow`, used by `pharos-proto`) can silently miss the whole
  click — no error, just a snapshot that quietly reflects pre-click
  state. `KeyPressEvent`/`KeyReleaseEvent` already solve this for the
  keyboard (split into two events, caller controls timing via `Wait()`);
  `docs/cimmerian_mouse_click_no_hold_gap.md` proposes the same shape for
  the mouse — `MouseButtonPressEvent`/`MouseButtonReleaseEvent`, additive
  only, `MouseClickEvent` unchanged. Not started; sized and ready to pick
  up.
- **`X11EventInjector`'s XTEST-over-XWayland gap (`cimmerian_wayland_xtest_injection_gap.md`)**
  — `LinuxUinputEventInjector` is the working fallback and is what's
  actually in use; the underlying "XTEST reports success but does
  nothing" issue on some compositors was never independently fixed,
  routed around instead. Still open if anyone wants `X11EventInjector`
  itself to be reliable rather than just avoided.
- **Corner-case for whoever picks up the click-hold work**: once
  `MouseButtonPressEvent`/`ReleaseEvent` exist, `pharos-proto`'s
  `tests/visual/pharos_toolbar.visual.test.cpp` has a real, previously-
  abandoned Explorer-row-selection / `ColorFilterDropdown`-open test
  attempt documented in its own trailing comment — a good first real
  consumer to validate the new primitive against once it lands.

## What changed this session

Nothing implemented here — `docs/cimmerian_mouse_click_no_hold_gap.md`
was filed after tracing the click-miss symptom (found while extending
`pharos-proto`'s visual regression suite) down to Cimmerian's own
`MouseClickEvent` handling, confirmed by reading all four injector
backends, not just the one that surfaced it.
