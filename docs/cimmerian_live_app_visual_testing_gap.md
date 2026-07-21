# Cimmerian — Gaps found using the visual-regression extension against a real, separately-launched app

> **Trigger:** trying to add `VISUAL_DESCRIBE`/`SEND`/`ASSERT_SNAPSHOT` visual
> regression tests (`docs/visual-regression-spec.md`) for `penumbra-proto`'s
> demo app — an SDL3 window, run as its own process, not a scratch window the
> test creates inline the way `test/visual.test.cpp` does. Four gaps found in
> the process; none of these is the XTEST-over-XWayland forwarding gap
> already covered by `docs/cimmerian_wayland_xtest_injection_gap.md` (that
> one is confirmed fixed/routed-around via `LinuxUinputEventInjector` per
> `docs/next_steps.md`) — these are new, found one layer earlier and one
> layer around that fix.
> **Status:** all four addressed. Gap 1: `docs/visual-regression-spec.md`
> now documents the X11/Wayland precondition, and `X11ScreenCapture::Capture()`
> emits a `TEST_LOG_WARN` pointing at it when window lookup fails. Gap 2:
> `WaitForWindowByTitle`/`WaitForWindowByPid`
> (`include/cimmerian/visual/window-lookup.hpp`, implemented per-platform —
> X11 window-tree polling for X11/Linux-uinput/Linux-auto,
> `EnumWindows`/`GetWindowThreadProcessId` for Win32,
> `CGWindowListCopyWindowInfo` for macOS) poll for a separately-launched
> app's window. Gap 3: `X11EventInjector`'s constructor
> no longer probes eagerly — `IEventInjector::Probe()` is a new virtual,
> `VisualTestRunner::SetInjector()` calls it once, deliberately. Gap 4:
> `IEventInjector::IsWindowFocused()` is a new virtual, `X11EventInjector`
> implements it via `XGetInputFocus`, and `VisualTestRunner::SendEvent()`
> warns (without refusing) when the target window isn't focused; the spec
> now documents that both backends are unscoped global OS input.
> **Environment:** KDE Plasma 6 (`kwin_wayland`), `XDG_SESSION_TYPE=wayland`,
> `libXtst` present, same machine/session as the Wayland gap doc.

---

## 1. X11 capture/injection can't see a window that was never an X11 window

**Symptom:** `penumbra-proto`'s demo (`SDL_Init(SDL_INIT_VIDEO)` +
`SDL_CreateWindowAndRenderer`, `src/Penumbra/Platform/PlatformWindow.cpp:69-74`)
launched with no env override is entirely absent from the X11 window tree —
confirmed with both `xwininfo -root -tree` and `xdotool search --name
"Penumbra Demo"` finding nothing, while the window is visibly on screen. SDL3
on a Wayland session defaults to its native Wayland video driver, so the
window is a native Wayland surface, never an XWayland client at all. Setting
`SDL_VIDEODRIVER=x11` before launch makes it appear in the X11 tree
immediately and become capturable/injectable as normal — confirmed working
once forced.

**Root cause:** `docs/visual-regression-spec.md`'s Platform Considerations
table (lines 350-362) documents *how* X11 capture/injection work
(`XGetImage`/`XTestFake*Event`, gated behind `if(UNIX AND NOT APPLE)` in
CMake) but says nothing about the precondition that the target window must
be an X11 (or XWayland) window in the first place. Read on its own, the spec
reads as "any on-screen window on Linux is capturable" — the native-Wayland
case isn't mentioned as something that silently doesn't apply. This is a
different failure mode from the XTEST-forwarding gap: that gap is about
injection failing *against a window XTEST can see*; this is about the window
not existing in X11's view at all, so `X11ScreenCapture::Capture()` would be
handed a window id that was never valid to begin with (this wasn't reached
in practice — the gap was caught before capture was attempted, by window
lookup finding nothing per gap 2 below).

**Why this can't be worked around locally today:** it can, partially — the
consuming app can be launched with `SDL_VIDEODRIVER=x11` forced. But nothing
in Cimmerian surfaces *that this is necessary* — a consumer who doesn't
already know their toolkit defaults to native Wayland gets silent "window
not found" failures (see gap 2) with no hint that the actual cause is one
env var away from fixed.

### Proposed fix

Not proposing a specific mechanism (same spirit as the Wayland XTEST doc) —
two directions that seem promising:

- Add a line to `docs/visual-regression-spec.md`'s Platform Considerations
  section stating the X11-mode precondition explicitly: the target window
  must be an X11/XWayland window, and common Wayland-native toolkits (SDL3,
  GTK4, Qt6 by default) need an explicit env var
  (`SDL_VIDEODRIVER=x11`, `GDK_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`) set
  before launch on a Wayland session.
- A cheap runtime check: if a window lookup (gap 2's proposed helper) or
  `X11ScreenCapture::Capture()` itself gets an invalid/nonexistent window id,
  emit a `TEST_LOG_WARN` suggesting the Wayland-native-surface explanation
  and the env var fix, the same way `X11EventInjector`'s constructor
  (`src/visual/platform/x11-event-injector.cpp:26-34`) already warns about
  the XTEST-forwarding case instead of just failing silently.

### What unblocks

Consumers testing a real Wayland-session app stop hitting an unexplained
"can't find/capture the window" wall and get pointed straight at the actual
one-line fix.

---

## 2. No helper for the "launch a separate app, find its window" pattern

**Symptom:** `test/visual.test.cpp`'s only example creates its own scratch
window inline (`CreateTestWindow()`, `test/visual.test.cpp:16-27`, via
`XCreateSimpleWindow`), so the `Window` handle is available immediately with
no discovery step. Testing a real, separately-launched application (spawn
the binary as a subprocess, wait for it to map its window, then get its
`Window` id — the realistic shape for testing almost any actual app, not a
disposable test fixture) has no equivalent helper anywhere under
`include/cimmerian/visual/` or `src/visual/` — confirmed via
`grep -rn "WaitForWindow\|FindWindow\|WindowByTitle\|WindowByPid"` across
both directories, zero matches.

**Root cause:** the extension's design (`docs/visual-regression-spec.md`
§"Screenshot"/§"VISUAL_DESCRIBE / VISUAL_TEST") takes the window handle as a
given, already-known `void*` at `VISUAL_DESCRIBE` registration time. Nothing
in the spec's Phase 2 ("End-to-end test: a small SDL or GLFW window as the
test subject") scope mentions how that SDL/GLFW window's handle actually
gets obtained by the test when the app is a normal separate process rather
than code the test links directly and calls into. Every consumer testing a
real app has to hand-roll `XQueryTree` + `XGetWMName`/`XGetClassHint`
polling themselves — exactly the low-level X11 code the extension's `visual/`
abstraction layer otherwise exists to hide (mirroring how `Screenshot`,
`SnapshotStore`, and `image-diff` already hide their own low-level details).

### Proposed fix

Not proposing exact signatures — this needs its own small design pass — but
the shape that would unblock the common case:

```cpp
// include/cimmerian/visual/window-lookup.hpp (illustrative — new file)
namespace Cimmerian::Visual {

// Polls until a window matching Title appears (or Timeout elapses), returning
// its native handle (Window on X11, HWND on Win32, NSWindow* on macOS) as
// void*, or nullptr on timeout. Mirrors WAIT()'s std::chrono::milliseconds
// convention (visual-test-macros.hpp).
void* WaitForWindowByTitle(const std::string& title,
                            std::chrono::milliseconds timeout);

// Same, matched by owning process id instead of title (e.g. the pid returned
// by whatever the consumer used to launch the subprocess) — useful when the
// window title isn't fixed/known ahead of time.
void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout);

} // namespace Cimmerian::Visual
```

### What unblocks

Testing any real, separately-launched application becomes a `SEND`/`WAIT`/
`ASSERT_SNAPSHOT` script plus one lookup call, instead of every consumer
re-implementing `XQueryTree` traversal from scratch.

---

## 3. `X11EventInjector`'s constructor moves the real system cursor unconditionally

**Symptom:** `X11EventInjector::X11EventInjector()`
(`src/visual/platform/x11-event-injector.cpp:10-35`) unconditionally calls
`this->functional = this->ProbeInjection();` at line 26.
`ProbeInjection()` (lines 40+) reads the real pointer position via
`XQueryPointer`, calls `XTestFakeMotionEvent` to move it to a probe point
(line 65), queries again, then calls `XTestFakeMotionEvent` a second time to
restore the original position (line 77) — all before the injector has been
asked to inject anything. On a real interactive desktop (as opposed to a
disposable CI VM with no human watching) this is a visible, momentary hijack
of the user's actual mouse cursor, purely as a side effect of constructing
the class — not from calling `Inject()`.

**Root cause:** the probe is unconditional and constructor-embedded rather
than opt-in. It's a reasonable fix for gap-doc
`cimmerian_wayland_xtest_injection_gap.md`'s false-pass problem (silent no-op
XTEST) — the `IsFunctional()` check it enables is genuinely valuable — but
doing it eagerly, every time, with no way to skip it, means even a consumer
who already knows (or doesn't care) whether XTEST works on their compositor
pays the same real-cursor side effect.

**Why this can't be worked around locally today:** it can't — the probe runs
inside the constructor before any consumer code executes.

### Proposed fix

Make the probe explicit rather than automatic — e.g. a separate
`bool Probe()` method the consumer (or `VisualTestRunner`, once, before a
whole suite run) calls deliberately, with the constructor leaving
`functional` unset (or optimistically `true`) until `Probe()` runs. This
keeps the exact same detection capability gap-doc 
`cimmerian_wayland_xtest_injection_gap.md` asked for, but makes the
real-cursor side effect something a consumer chooses to trigger (and can
time — e.g. warn the user first) rather than something that happens the
instant `X11EventInjector` is constructed.

### What unblocks

Consumers running visual tests on their own live desktop (not just CI) don't
get an unexplained, unannounced cursor jump the moment the test process
starts.

---

## 4. Neither injection backend is scoped to the window under test

**Symptom:** `X11EventInjector::Inject()`'s pointer/button calls
(`XTestFakeMotionEvent`/`XTestFakeButtonEvent`,
`src/visual/platform/x11-event-injector.cpp:99-103`) go through the X server
generally — wherever the (server-global) pointer ends up is whatever
receives the click, not necessarily the window under test.
`LinuxUinputEventInjector` (`src/visual/platform/linux-uinput-event-injector.cpp`)
goes a level further: it opens `/dev/uinput` directly (line 79) and injects
at the kernel input-device layer via `ioctl` (lines 87-94 register the
device's key/button capabilities) — genuinely global OS input, indistinguishable
from real hardware, with no concept of "target window" at all.

**Root cause:** both backends implement `docs/visual-regression-spec.md`'s
own prescribed mechanism for their platform (the spec's Platform
Considerations table, lines 352-358, literally names these APIs) — this
isn't a bug in either implementation, it's that neither the XTEST/X11 nor
the Linux kernel input layer has a "scoped to this window" concept the way
e.g. a browser automation tool's isolated page context does. On a real
multi-window desktop, if the target window loses real OS focus during a
`WAIT()` (another app's notification, a user's own alt-tab, a background
process raising a window), the next `SEND()` goes wherever focus actually is
— not necessarily anywhere near the coordinates the test intended, since
X11/uinput coordinates are screen-absolute, not window-relative once they
leave `EventSequence`.

**Why this can't be worked around locally today:** a consumer can reduce the
risk (run the test on a machine/session with nothing else happening, or in
a headless `Xvfb`), but nothing in the library itself detects or guards
against a focus change mid-sequence.

### Proposed fix

Not proposing a mechanism — flagging it as a documentation gap at minimum:
`docs/visual-regression-spec.md` doesn't currently warn that both backends
are real, global OS input, unscoped to the target window, and that running
visual regression tests against a live, multi-window developer desktop
(rather than a dedicated `Xvfb`/CI display) carries a real risk of a stray
`SEND()` landing in an unrelated window if focus shifts mid-test. A
concrete, low-effort option worth recording for whoever picks this up:
`X11EventInjector::Inject()` could check window focus (`XGetInputFocus`)
before injecting and warn (or refuse) if the target window isn't focused,
narrowing — though not eliminating — the window for a focus-shift race.

### What unblocks

At minimum, consumers make an informed choice about *where* to run visual
regression tests (dedicated display vs. live desktop) instead of discovering
the unscoped-input risk by an actual stray click.

---

## 5. Explicitly not requested

- **A fully window-scoped/sandboxed input backend** (the Playwright/Selenium
  model) — real architectural work, not a small fix; gap 4 only asks that
  the *existing* unscoped behavior be documented and, optionally, focus-
  checked before injecting.
- **Auto-detection of the Wayland-native-surface case** (gap 1) beyond a
  warning on lookup/capture failure — actually forcing a toolkit to switch
  video drivers out from under a consumer isn't this library's place.
- **Re-litigating `cimmerian_wayland_xtest_injection_gap.md`** — that gap
  and its fix (`LinuxUinputEventInjector`, the `IsFunctional()` probe) are
  unrelated to and unaffected by anything in this doc.
