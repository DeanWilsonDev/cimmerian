#include "cimmerian/visual.hpp"
#include <X11/Xlib.h>
#include <chrono>

using namespace std::chrono_literals;

namespace {

// A tiny throwaway X11 window that exists purely so the visual regression
// tests below have something real to screenshot, without touching the
// user's actual desktop windows.
struct TestWindowHandle {
  Display* display;
  Window window;
};

TestWindowHandle CreateTestWindow()
{
  Display* display = XOpenDisplay(nullptr);
  const int screen = DefaultScreen(display);
  Window window = XCreateSimpleWindow(
      display, RootWindow(display, screen), 10, 10, 200, 150, 1, BlackPixel(display, screen),
      WhitePixel(display, screen)
  );
  XMapWindow(display, window);
  XSync(display, False);
  return {display, window};
}

TestWindowHandle& GetTestWindow()
{
  static TestWindowHandle handle = CreateTestWindow();
  return handle;
}

} // namespace

VISUAL_DESCRIBE("Scratch Window", reinterpret_cast<void*>(GetTestWindow().window), {
  VISUAL_TEST("looks the same between runs", {
    WAIT(20ms);
    ASSERT_SNAPSHOT("initial-state", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });
});
