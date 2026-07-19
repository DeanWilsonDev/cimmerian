#include "cimmerian/visual/platform/macos-event-injector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace Cimmerian::Visual {

namespace {

CGMouseButton ToCGMouseButton(int button)
{
  switch (button) {
    case 2: return kCGMouseButtonCenter;
    case 3: return kCGMouseButtonRight;
    default: return kCGMouseButtonLeft;
  }
}

} // namespace

MacOSEventInjector::MacOSEventInjector()
{
  if (!AXIsProcessTrusted()) {
    throw std::runtime_error(
        "MacOSEventInjector: process is not trusted for Accessibility access; "
        "grant it in System Settings > Privacy & Security > Accessibility"
    );
  }
}

void MacOSEventInjector::Inject(const UIEvent& event)
{
  std::visit(
      [](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, MouseMoveEvent>) {
          CGPoint point = CGPointMake(e.x, e.y);
          CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, point, kCGMouseButtonLeft);
          CGEventPost(kCGHIDEventTap, move);
          CFRelease(move);
        }
        else if constexpr (std::is_same_v<T, MouseClickEvent>) {
          CGPoint point = CGPointMake(e.x, e.y);
          CGMouseButton cgButton = ToCGMouseButton(e.button);
          CGEventType downType = (e.button == 3) ? kCGEventRightMouseDown : kCGEventLeftMouseDown;
          CGEventType upType = (e.button == 3) ? kCGEventRightMouseUp : kCGEventLeftMouseUp;

          CGEventRef down = CGEventCreateMouseEvent(nullptr, downType, point, cgButton);
          CGEventPost(kCGHIDEventTap, down);
          CFRelease(down);

          CGEventRef up = CGEventCreateMouseEvent(nullptr, upType, point, cgButton);
          CGEventPost(kCGHIDEventTap, up);
          CFRelease(up);
        }
        else if constexpr (std::is_same_v<T, MouseScrollEvent>) {
          CGEventRef scroll =
              CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, e.deltaY, e.deltaX);
          CGEventPost(kCGHIDEventTap, scroll);
          CFRelease(scroll);
        }
        else if constexpr (std::is_same_v<T, KeyPressEvent>) {
          CGEventRef keyDown = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(e.keyCode), true);
          CGEventPost(kCGHIDEventTap, keyDown);
          CFRelease(keyDown);
        }
        else if constexpr (std::is_same_v<T, KeyReleaseEvent>) {
          CGEventRef keyUp = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(e.keyCode), false);
          CGEventPost(kCGHIDEventTap, keyUp);
          CFRelease(keyUp);
        }
        else if constexpr (std::is_same_v<T, WaitEvent>) {
          // Handled by VisualTestRunner::SendEvent before reaching here.
        }
      },
      event
  );
}

} // namespace Cimmerian::Visual
