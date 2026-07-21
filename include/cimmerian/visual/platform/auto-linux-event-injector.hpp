#pragma once

#include <memory>
#include "linux-uinput-event-injector.hpp"
#include "x11-event-injector.hpp"

namespace Cimmerian::Visual {

// Tries X11EventInjector (XTEST) first; if Probe() finds XTEST reports
// success without the input actually reaching the compositor's input
// pipeline (see docs/cimmerian_wayland_xtest_injection_gap.md), falls back
// to LinuxUinputEventInjector, which operates below both X11 and Wayland at
// the kernel input-device layer and isn't subject to that gap. Selected via
// CIMMERIAN_VISUAL_PLATFORM=Linux-auto.
//
// This can't make a compositor forward XTEST-over-XWayland input it has
// chosen not to forward - that's the compositor's call, not something
// fixable from the X11 client side - but it means a consumer no longer has
// to pick a backend up front and hope: construction is always the cheap
// X11EventInjector path (no root/udev requirement), and the heavier uinput
// setup (needs /dev/uinput access) is only paid if XTEST actually turns out
// to be non-functional on the current compositor, discovered via the same
// deliberate Probe() call VisualTestRunner::SetInjector() already makes
// (see docs/cimmerian_live_app_visual_testing_gap.md gap 3).
class AutoLinuxEventInjector : public IEventInjector {
public:
  AutoLinuxEventInjector();
  ~AutoLinuxEventInjector() override = default;

  AutoLinuxEventInjector(const AutoLinuxEventInjector&) = delete;
  AutoLinuxEventInjector& operator=(const AutoLinuxEventInjector&) = delete;

  void Inject(const UIEvent& event) override;
  bool IsFunctional() const override;
  bool Probe() override;
  bool IsWindowFocused(void* windowHandle) const override;

private:
  X11EventInjector x11Injector;
  std::unique_ptr<LinuxUinputEventInjector> uinputFallback;
  bool usingFallback = false;
};

} // namespace Cimmerian::Visual
