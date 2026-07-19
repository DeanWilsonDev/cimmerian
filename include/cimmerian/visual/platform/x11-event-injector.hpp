#pragma once

#include "../i-event-injector.hpp"

// Forward-declared instead of pulling in <X11/Xlib.h> here so consumers who
// merely construct/pass this class around don't need Xlib headers.
struct _XDisplay;

namespace Cimmerian::Visual {

// Injects via XTestFakeMotionEvent / XTestFakeButtonEvent / XTestFakeKeyEvent
// (libXtst). Requires the XTEST extension on the target X server.
class X11EventInjector : public IEventInjector {
public:
  X11EventInjector();
  ~X11EventInjector() override;

  X11EventInjector(const X11EventInjector&) = delete;
  X11EventInjector& operator=(const X11EventInjector&) = delete;

  void Inject(const UIEvent& event) override;

private:
  _XDisplay* display;
};

} // namespace Cimmerian::Visual
