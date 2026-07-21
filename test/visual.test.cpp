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

// Stands in for the consumer's own router/view-model: NAVIGATE("...") below
// reaches this instead of clicking through a menu with SEND(). Real
// consumers would call into their own app's navigation API here (see
// docs/cimmerian_navigation_without_platform_input_proposal.md Proposal A).
void NavigateScratchWindow(const std::string& screenKey)
{
  TestWindowHandle& handle = GetTestWindow();
  const int screen = DefaultScreen(handle.display);
  unsigned long color = WhitePixel(handle.display, screen);
  if (screenKey == "red") {
    color = 0xCC3333;
  }
  else if (screenKey == "blue") {
    color = 0x3355CC;
  }
  XSetWindowBackground(handle.display, handle.window, color);
  XClearWindow(handle.display, handle.window);
  XSync(handle.display, False);
}

CIMMERIAN_MAYBE_UNUSED static const bool _registerScratchNavigator = []() {
  Cimmerian::Visual::ActiveNavigationDriver::GetInstance().Set(NavigateScratchWindow);
  return true;
}();

// Stands in for the consumer's MountComponent<T> (Proposal B) - constructs
// a fresh, dedicated host window per test rather than reusing the shared
// scratch window above, and hands ASSERT_SNAPSHOT that window's handle
// directly instead of relying on a VISUAL_DESCRIBE-level one.
struct ScratchComponentHost {
  Display* display;
  Window window;

  void* WindowHandle() const { return reinterpret_cast<void*>(window); }
};

ScratchComponentHost MountScratchComponent(unsigned long backgroundColor)
{
  Display* display = XOpenDisplay(nullptr);
  const int screen = DefaultScreen(display);
  Window window = XCreateSimpleWindow(
      display, RootWindow(display, screen), 10, 10, 120, 80, 1, BlackPixel(display, screen), backgroundColor
  );
  XMapWindow(display, window);
  XSync(display, False);
  return {display, window};
}

void UnmountScratchComponent(const ScratchComponentHost& host)
{
  XDestroyWindow(host.display, host.window);
  XCloseDisplay(host.display);
}

} // namespace

VISUAL_DESCRIBE("Scratch Window", reinterpret_cast<void*>(GetTestWindow().window), {
  VISUAL_TEST("looks the same between runs", {
    WAIT(20ms);
    ASSERT_SNAPSHOT("initial-state", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });

  VISUAL_TEST("reaches a screen via NAVIGATE instead of SEND", {
    NAVIGATE("red");
    WAIT(20ms);
    ASSERT_SNAPSHOT("red-screen", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });
});

VISUAL_DESCRIBE_COMPONENT("Scratch Component", {
  VISUAL_TEST("default background", {
    ScratchComponentHost host = MountScratchComponent(0x33AA55);
    WAIT(20ms);
    ASSERT_SNAPSHOT("default", host.WindowHandle(), Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
    UnmountScratchComponent(host);
  });
});
