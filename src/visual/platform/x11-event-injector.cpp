#include "cimmerian/visual/platform/x11-event-injector.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <cstdlib>
#include <stdexcept>

namespace Cimmerian::Visual {

X11EventInjector::X11EventInjector()
    : display(nullptr)
{
  Display* opened = XOpenDisplay(nullptr);
  if (!opened) {
    throw std::runtime_error("X11EventInjector: unable to open X display (is $DISPLAY set?)");
  }

  int eventBase = 0, errorBase = 0, majorVersion = 0, minorVersion = 0;
  if (!XTestQueryExtension(opened, &eventBase, &errorBase, &majorVersion, &minorVersion)) {
    XCloseDisplay(opened);
    throw std::runtime_error("X11EventInjector: XTEST extension is not available on this X server");
  }

  this->display = reinterpret_cast<_XDisplay*>(opened);
}

X11EventInjector::~X11EventInjector()
{
  if (this->display) {
    XCloseDisplay(reinterpret_cast<Display*>(this->display));
  }
}

void X11EventInjector::Inject(const UIEvent& event)
{
  Display* dpy = reinterpret_cast<Display*>(this->display);

  std::visit(
      [dpy](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, MouseMoveEvent>) {
          XTestFakeMotionEvent(dpy, -1, e.x, e.y, CurrentTime);
        }
        else if constexpr (std::is_same_v<T, MouseClickEvent>) {
          XTestFakeMotionEvent(dpy, -1, e.x, e.y, CurrentTime);
          XTestFakeButtonEvent(dpy, static_cast<unsigned int>(e.button), True, CurrentTime);
          XTestFakeButtonEvent(dpy, static_cast<unsigned int>(e.button), False, CurrentTime);
        }
        else if constexpr (std::is_same_v<T, MouseScrollEvent>) {
          XTestFakeMotionEvent(dpy, -1, e.x, e.y, CurrentTime);
          // X11 wheel scroll is modelled as button 4 (up) / 5 (down) clicks.
          const unsigned int verticalButton = e.deltaY < 0 ? 5 : 4;
          for (int i = 0; i < std::abs(e.deltaY); ++i) {
            XTestFakeButtonEvent(dpy, verticalButton, True, CurrentTime);
            XTestFakeButtonEvent(dpy, verticalButton, False, CurrentTime);
          }
          const unsigned int horizontalButton = e.deltaX < 0 ? 7 : 6;
          for (int i = 0; i < std::abs(e.deltaX); ++i) {
            XTestFakeButtonEvent(dpy, horizontalButton, True, CurrentTime);
            XTestFakeButtonEvent(dpy, horizontalButton, False, CurrentTime);
          }
        }
        else if constexpr (std::is_same_v<T, KeyPressEvent>) {
          XTestFakeKeyEvent(dpy, static_cast<unsigned int>(e.keyCode), True, CurrentTime);
        }
        else if constexpr (std::is_same_v<T, KeyReleaseEvent>) {
          XTestFakeKeyEvent(dpy, static_cast<unsigned int>(e.keyCode), False, CurrentTime);
        }
        else if constexpr (std::is_same_v<T, WaitEvent>) {
          // Handled by VisualTestRunner::SendEvent before it reaches the
          // injector; nothing to do here.
        }
      },
      event
  );

  XFlush(dpy);
}

} // namespace Cimmerian::Visual
