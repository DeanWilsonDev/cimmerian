# Cimmerian — Gap: `LinuxUinputEventInjector` has no functional self-check, unlike `X11EventInjector`

> **Trigger:** using `pharos-proto`'s `CIMMERIAN_VISUAL_PLATFORM=Linux-uinput`
> build (`pharos-proto/cmake/Dependencies.cmake:182`, chosen specifically
> *because* of `docs/cimmerian_wayland_xtest_injection_gap.md` — XTEST
> doesn't reach this compositor's real input pipeline) to re-attempt a
> click-driven visual-regression test
> (`pharos-proto/docs/pharos_click_hold_selection_test_feature_request.md`).
> The test built, ran, and captured a "passing" golden with no error or
> warning anywhere — but the click never actually selected anything. Root
> cause traced to synthetic `/dev/uinput` input not reaching the real
> pointer in that session at all, with nothing in Cimmerian's own API
> surfacing that fact.
> **Status:** not addressed — reporting the gap, not yet fixed.
> **Environment:** KDE Plasma (`kwin_wayland`), `XDG_SESSION_TYPE=wayland`,
> a sandboxed shell session distinct from `docs/cimmerian_wayland_xtest_
> injection_gap.md`'s and `docs/cimmerian_live_app_visual_testing_gap.md`'s
> own (both plain interactive KDE/XWayland sessions) — this one is a
> tool-invoked shell inside an agentic coding assistant, on the same
> physical machine and the same real KWin/XWayland instance those other
> two docs used.

---

## 1. What was observed

A `pharos-proto` visual test held a real press/release
(`SEND(MouseButtonPress(x,y))` / `WAIT(300ms)` /
`SEND(MouseButtonRelease())`, cimmerian commit `91ac684` onward) at
pixel-verified-correct coordinates over a specific UI row. The app never
registered the click — no hover tint, no selection state change — despite
the test itself reporting `[NEW] golden captured` with no failure, warning,
or diagnostic of any kind.

Isolated with a standalone program, independent of both Cimmerian and
`pharos-proto`, reproducing `LinuxUinputEventInjector`'s own constructor
sequence (`src/visual/platform/linux-uinput-event-injector.cpp:70-137`) by
hand:

```cpp
int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);   // succeeds
// ... UI_SET_EVBIT/UI_SET_KEYBIT/UI_SET_ABSBIT/UI_DEV_SETUP/UI_ABS_SETUP,
// all ioctl calls return 0 (success), none throw
ioctl(fd, UI_DEV_CREATE);                                // succeeds
// device now visible in /proc/bus/input/devices:
//   N: Name="Test Virtual Input"
//   H: Handlers=event18 mouse3 js0
sleep(500ms);   // settle time, same as the real class's own 200ms sleep
// write EV_ABS/ABS_X, EV_ABS/ABS_Y, EV_SYN/SYN_REPORT to move to (300,300)
```

`xdotool getmouselocation` read immediately before and after: **identical
position**, zero movement. Retried with a plain `REL_X`/`REL_Y` mouse
device (no `ABS`, no `INPUT_PROP_DIRECT` — ruling out the touch/tablet
device-classification explanation) sending twenty small relative deltas
over 400ms: still zero movement. The kernel accepts and registers the
virtual device (it appears in `/proc/bus/input/devices` with real evdev
handler nodes) but nothing in this particular session's Wayland/KWin
pointer pipeline actually consumes its events — most likely a
logind/seat-assignment gap specific to how this sandboxed shell's session
is scoped, separate from the seat the real, visible KWin instance owns,
even though `$DISPLAY`/`$WAYLAND_DISPLAY` both connect successfully and
`xdotool` (an X11 protocol client, not an input-injection path) reads the
real pointer position fine.

## 2. Why this is a real Cimmerian gap, not just an environment quirk to route around

`docs/cimmerian_wayland_xtest_injection_gap.md` hit the equivalent failure
mode for `X11EventInjector` (XTEST reports success, does nothing) and
fixed it two ways: `LinuxUinputEventInjector` as a working alternative
*and* a real `Probe()`/`IsFunctional()` self-check
(`src/visual/platform/x11-event-injector.cpp:29-88`,
`ProbeInjection()` does a before/after `XQueryPointer` round-trip around a
real `XTestFakeMotionEvent`, restoring the original position afterward).
`docs/cimmerian_live_app_visual_testing_gap.md` gap 3 made that check an
explicit, deliberate call (`VisualTestRunner::SetInjector()` calls
`Probe()` once) rather than an unannounced constructor side effect.

`LinuxUinputEventInjector` never received the equivalent treatment:

```cpp
// include/cimmerian/visual/i-event-injector.hpp:19
virtual bool IsFunctional() const { return true; }
// include/cimmerian/visual/i-event-injector.hpp:29
virtual bool Probe() { return this->IsFunctional(); }
```

`LinuxUinputEventInjector` (`include/cimmerian/visual/platform/
linux-uinput-event-injector.hpp:22-50`) overrides neither — it inherits
the base class's optimistic `true`. The class's own constructor already
treats "the device was created without an ioctl error" as success (throws
only on `open()`/`UI_DEV_SETUP`/`UI_ABS_SETUP`/`UI_DEV_CREATE` failure,
`linux-uinput-event-injector.cpp:78-137`) — exactly the same category of
false confidence `ProbeInjection()` was written to catch on the X11 side:
the kernel accepting a device and the compositor actually routing its
events to the real pointer are two different, independently-failing
things, same as "XTEST call returns `True`" and "the compositor forwards
it" were two different things in the original gap doc.

The consequence matches that doc's own §3 exactly: `SEND(...)` silently
no-ops, `VISUAL_TEST` reports a clean pass (or, worse, a freshly-captured
"golden" that's actually just the *pre-click* state, indistinguishable
from a real one until a human reviews the image by eye), and nothing in
Cimmerian's own output — no `TEST_LOG_WARN`, no exception, no return
value — hints that injection never reached the target.

## 3. Proposed fix

Mirror `X11EventInjector::ProbeInjection()`'s shape, adapted for
`/dev/uinput`: `XQueryPointer` already works for *reading* pointer
position in this same environment (confirmed above — `xdotool
getmouselocation` and the standalone probe both read real positions
successfully; it's only uinput's *writes* that don't land), so the same
before/after comparison technique applies directly:

```cpp
// src/visual/platform/linux-uinput-event-injector.cpp (illustrative)
bool LinuxUinputEventInjector::Probe()
{
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    return this->functional = true;  // can't check; don't block on it
  }

  Window root = DefaultRootWindow(dpy), child;
  int origX = 0, origY = 0, winX = 0, winY = 0;
  unsigned int mask = 0;
  XQueryPointer(dpy, root, &child, &child, &origX, &origY, &winX, &winY, &mask);

  const int probeX = (origX + this->screenWidth / 2) % std::max(this->screenWidth, 1);
  const int probeY = (origY + this->screenHeight / 2) % std::max(this->screenHeight, 1);
  this->MoveTo(probeX, probeY);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));  // let the seat catch up

  int movedX = 0, movedY = 0;
  XQueryPointer(dpy, root, &child, &child, &movedX, &movedY, &winX, &winY, &mask);
  this->functional = (movedX != origX) || (movedY != origY);

  this->MoveTo(origX, origY);  // restore, regardless of outcome
  XCloseDisplay(dpy);

  if (!this->functional) {
    TEST_LOG_WARN(
        "LinuxUinputEventInjector: /dev/uinput device created successfully "
        "but events aren't reaching the real pointer on this session - "
        "possibly a logind/seat-assignment gap (see "
        "docs/cimmerian_uinput_no_functional_check_gap.md). SEND() in "
        "visual tests will silently no-op."
    );
  }
  return this->functional;
}

bool LinuxUinputEventInjector::IsFunctional() const { return this->functional; }
```

Same caveat `docs/cimmerian_wayland_xtest_injection_gap.md` §5 already
states for its own fix: this detects the problem, it doesn't fix the
underlying seat/session routing — there's nothing a client-side library
can do about a compositor or session manager that won't route synthetic
input to a given process's requests. But a loud, queryable "this won't
work here" beats a silent false pass every time.

Also worth considering, lower priority: `AutoLinuxEventInjector`
(`src/visual/platform/auto-linux-event-injector.cpp`) already has the
scaffolding to prefer whichever backend actually probes functional — once
`LinuxUinputEventInjector::Probe()` exists, `Linux-auto` could try XTEST,
then uinput, and warn loudly (rather than silently returning `true` from
the base class either way) if *neither* backend's probe passes, which is
exactly this doc's scenario.

## 4. Explicitly not requested

- **Diagnosing *why* this particular sandboxed session's seat doesn't
  route uinput events** — that's specific to this one environment
  (whatever process/session isolation an agentic coding tool's shell runs
  under vs. a normal interactive terminal), not something Cimmerian's
  library code can detect or fix beyond reporting it.
- **Re-litigating `docs/cimmerian_wayland_xtest_injection_gap.md`** or
  `docs/cimmerian_live_app_visual_testing_gap.md` — this doc is additive:
  the fixes those two shipped (uinput backend, `X11EventInjector::Probe()`,
  `AutoLinuxEventInjector`) all worked as designed; this is the one
  remaining backend that never got the same self-check treatment.

## 5. What unblocks when this lands

A `pharos-proto` (or any consumer's) `VISUAL_TEST` using
`CIMMERIAN_VISUAL_PLATFORM=Linux-uinput` gets the same honesty
`X11EventInjector` already has: call `Probe()` once, check
`IsFunctional()`, and know — instead of discovering by hand-reviewing a
captured golden image — whether a captured "pass" reflects a real
interaction or a silently-skipped one.
