#include "scratch-window.hpp"
#include <X11/Xlib.h>

namespace CimmerianTest {

namespace {

struct WindowContext {
  Display* display;
  Window window;
};

WindowContext& GetPersistentWindow()
{
  static WindowContext context = [] {
    Display* display = XOpenDisplay(nullptr);
    const int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen), 10, 10, 200, 150, 1, BlackPixel(display, screen),
        WhitePixel(display, screen)
    );
    XMapWindow(display, window);
    XSync(display, False);
    return WindowContext {display, window};
  }();
  return context;
}

} // namespace

void* GetScratchWindowHandle()
{
  return reinterpret_cast<void*>(GetPersistentWindow().window);
}

void NavigateScratchWindow(const std::string& screenKey)
{
  WindowContext& context = GetPersistentWindow();
  const int screen = DefaultScreen(context.display);
  unsigned long color = WhitePixel(context.display, screen);
  if (screenKey == "red") {
    color = 0xCC3333;
  }
  else if (screenKey == "blue") {
    color = 0x3355CC;
  }
  XSetWindowBackground(context.display, context.window, color);
  XClearWindow(context.display, context.window);
  XSync(context.display, False);
}

ScratchComponentHost MountScratchComponent(std::uint32_t backgroundColorRGB)
{
  Display* display = XOpenDisplay(nullptr);
  const int screen = DefaultScreen(display);
  Window window = XCreateSimpleWindow(
      display, RootWindow(display, screen), 10, 10, 120, 80, 1, BlackPixel(display, screen), backgroundColorRGB
  );
  XMapWindow(display, window);
  XSync(display, False);

  auto* context = new WindowContext {display, window};

  ScratchComponentHost host;
  host.nativeHandle = context;
  host.captureHandle = reinterpret_cast<void*>(window);
  return host;
}

void UnmountScratchComponent(const ScratchComponentHost& host)
{
  auto* context = static_cast<WindowContext*>(host.nativeHandle);
  XDestroyWindow(context->display, context->window);
  XCloseDisplay(context->display);
  delete context;
}

} // namespace CimmerianTest
