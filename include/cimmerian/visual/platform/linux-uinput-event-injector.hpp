#pragma once

#include "../i-event-injector.hpp"

namespace Cimmerian::Visual {

// Injects by creating a virtual absolute-pointer + keyboard device on
// /dev/uinput (the same kernel input-device mechanism ydotool/libei use).
// This operates below both X11 and Wayland at the evdev layer, so it isn't
// subject to a compositor's XTEST-forwarding behavior at all (see
// docs/cimmerian_wayland_xtest_injection_gap.md) - the standard fallback for
// Wayland sessions where XTEST-over-XWayland doesn't reach the seat.
//
// Requires write access to /dev/uinput (root, or a udev rule granting the
// `input` group) - the constructor throws if that access isn't available.
//
// The kernel accepting the virtual device (constructor success) is not proof
// the compositor/session actually routes its events to the real seat - some
// sandboxed/session-isolated environments create the device fine but nothing
// ever consumes its events (see
// docs/cimmerian_uinput_no_functional_check_gap.md). Probe() runs a
// before/after XQueryPointer check, same technique as
// X11EventInjector::ProbeInjection(), to detect this; IsFunctional() reports
// the last result.
//
// KeyPressEvent/KeyReleaseEvent::keyCode is interpreted as an X11 keycode
// (matching X11EventInjector's convention) and converted to the
// corresponding Linux evdev code via the standard `evdev` XKB rule offset
// (evdev code = X11 keycode - 8), which holds on essentially all modern
// Linux desktops.
class LinuxUinputEventInjector : public IEventInjector {
public:
  // width/height define the pixel coordinate space MouseMoveEvent /
  // MouseClickEvent / MouseScrollEvent arrive in - the virtual device's
  // absolute axis range is set to match, so incoming coordinates map
  // 1:1 onto screen pixels. Pass 0 for either to auto-detect: first the
  // CIMMERIAN_VISUAL_SCREEN_WIDTH / CIMMERIAN_VISUAL_SCREEN_HEIGHT
  // environment variables, then a plain X11/XWayland geometry query
  // (DisplayWidth/DisplayHeight - a read-only protocol query, not XTEST, so
  // it works even where XTEST-forwarding doesn't), then a 1920x1080
  // fallback.
  explicit LinuxUinputEventInjector(int width = 0, int height = 0);
  ~LinuxUinputEventInjector() override;

  LinuxUinputEventInjector(const LinuxUinputEventInjector&) = delete;
  LinuxUinputEventInjector& operator=(const LinuxUinputEventInjector&) = delete;

  void Inject(const UIEvent& event) override;
  bool IsFunctional() const override { return this->functional; }

  // Moves the virtual pointer via /dev/uinput and checks - via a real X11
  // XQueryPointer round-trip - whether it actually reached the real pointer,
  // restoring the original position afterward. The kernel accepting the
  // uinput device (see the constructor) is not proof the compositor/session
  // actually routes its events to the real seat (see
  // docs/cimmerian_uinput_no_functional_check_gap.md). Returns the new
  // IsFunctional() value.
  bool Probe() override;

private:
  int fd;
  int screenWidth;
  int screenHeight;
  bool functional;

  void MoveTo(int x, int y);
  void EmitKey(int code, int value);
  void EmitScroll(int code, int value);
  void SyncReport();
};

} // namespace Cimmerian::Visual
