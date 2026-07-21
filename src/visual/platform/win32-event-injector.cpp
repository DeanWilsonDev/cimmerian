#include "cimmerian/visual/platform/win32-event-injector.hpp"
#include <windows.h>
#include <cstdlib>
#include <type_traits>

namespace Cimmerian::Visual {

namespace {

void SendMouseMove(int x, int y)
{
  const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  INPUT input {};
  input.type = INPUT_MOUSE;
  input.mi.dx = static_cast<LONG>((static_cast<long long>(x) * 65535) / (screenWidth > 0 ? screenWidth : 1));
  input.mi.dy = static_cast<LONG>((static_cast<long long>(y) * 65535) / (screenHeight > 0 ? screenHeight : 1));
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  SendInput(1, &input, sizeof(INPUT));
}

void SendMouseButton(int button, bool down)
{
  INPUT input {};
  input.type = INPUT_MOUSE;
  if (button == 3) {
    input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
  }
  else if (button == 2) {
    input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
  }
  else {
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
  }
  SendInput(1, &input, sizeof(INPUT));
}

void SendMouseWheel(int deltaX, int deltaY)
{
  if (deltaY != 0) {
    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(deltaY * WHEEL_DELTA);
    SendInput(1, &input, sizeof(INPUT));
  }
  if (deltaX != 0) {
    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    input.mi.mouseData = static_cast<DWORD>(deltaX * WHEEL_DELTA);
    SendInput(1, &input, sizeof(INPUT));
  }
}

void SendKey(int keyCode, bool down)
{
  INPUT input {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = static_cast<WORD>(keyCode);
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(INPUT));
}

} // namespace

void Win32EventInjector::Inject(const UIEvent& event)
{
  std::visit(
      [](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, MouseMoveEvent>) {
          SendMouseMove(e.x, e.y);
        }
        else if constexpr (std::is_same_v<T, MouseClickEvent>) {
          SendMouseMove(e.x, e.y);
          SendMouseButton(e.button, true);
          SendMouseButton(e.button, false);
        }
        else if constexpr (std::is_same_v<T, MouseButtonPressEvent>) {
          SendMouseMove(e.x, e.y);
          SendMouseButton(e.button, true);
        }
        else if constexpr (std::is_same_v<T, MouseButtonReleaseEvent>) {
          SendMouseButton(e.button, false);
        }
        else if constexpr (std::is_same_v<T, MouseScrollEvent>) {
          SendMouseMove(e.x, e.y);
          SendMouseWheel(e.deltaX, e.deltaY);
        }
        else if constexpr (std::is_same_v<T, KeyPressEvent>) {
          SendKey(e.keyCode, true);
        }
        else if constexpr (std::is_same_v<T, KeyReleaseEvent>) {
          SendKey(e.keyCode, false);
        }
        else if constexpr (std::is_same_v<T, WaitEvent>) {
          // Handled by VisualTestRunner::SendEvent before reaching here.
        }
      },
      event
  );
}

} // namespace Cimmerian::Visual
