#pragma once

#include "../i-event-injector.hpp"

// Forward-declared instead of pulling in <X11/Xlib.h> here so consumers who
// merely construct/pass this class around don't need Xlib headers.
struct _XDisplay;

namespace Cimmerian::Visual {

// Injects via XTestFakeMotionEvent / XTestFakeButtonEvent / XTestFakeKeyEvent
// (libXtst). Requires the XTEST extension on the target X server.
//
// XTEST reporting success is not proof the input actually arrived: some
// Wayland compositors accept XTEST calls from XWayland clients but never
// forward them into the real input pipeline (see
// docs/cimmerian_wayland_xtest_injection_gap.md). The constructor runs a
// before/after XQueryPointer probe to detect this; IsFunctional() reports
// the result.
class X11EventInjector : public IEventInjector {
public:
  X11EventInjector();
  ~X11EventInjector() override;

  X11EventInjector(const X11EventInjector&) = delete;
  X11EventInjector& operator=(const X11EventInjector&) = delete;

  void Inject(const UIEvent& event) override;
  bool IsFunctional() const override { return this->functional; }

private:
  _XDisplay* display;
  bool functional;

  // Moves the pointer via XTEST and checks whether it actually moved,
  // restoring the original position afterward. Throws only for setup
  // failures (no display, no XTEST) — a non-functional result is reported
  // via IsFunctional(), not an exception, since XTEST-present-but-inert is a
  // usable (if degraded) state a caller may want to detect and act on.
  bool ProbeInjection();
};

} // namespace Cimmerian::Visual
