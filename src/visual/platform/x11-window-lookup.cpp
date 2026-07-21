#include "cimmerian/visual/window-lookup.hpp"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

namespace {

constexpr std::chrono::milliseconds POLL_INTERVAL {50};

std::string GetWindowTitle(Display* display, Window window)
{
  Atom netWmName = XInternAtom(display, "_NET_WM_NAME", True);
  Atom utf8String = XInternAtom(display, "UTF8_STRING", True);
  if (netWmName != None && utf8String != None) {
    Atom actualType;
    int actualFormat = 0;
    unsigned long itemCount = 0, bytesAfter = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(
            display, window, netWmName, 0, 1024, False, utf8String, &actualType, &actualFormat, &itemCount,
            &bytesAfter, &data
        ) == Success &&
        data) {
      std::string title(reinterpret_cast<char*>(data), itemCount);
      XFree(data);
      if (!title.empty()) {
        return title;
      }
    }
  }

  char* name = nullptr;
  if (XFetchName(display, window, &name) && name) {
    std::string title(name);
    XFree(name);
    return title;
  }
  return "";
}

long GetWindowPid(Display* display, Window window)
{
  Atom netWmPid = XInternAtom(display, "_NET_WM_PID", True);
  if (netWmPid == None) {
    return -1;
  }

  Atom actualType;
  int actualFormat = 0;
  unsigned long itemCount = 0, bytesAfter = 0;
  unsigned char* data = nullptr;
  long pid = -1;
  if (XGetWindowProperty(
          display, window, netWmPid, 0, 1, False, XA_CARDINAL, &actualType, &actualFormat, &itemCount,
          &bytesAfter, &data
      ) == Success &&
      data) {
    if (itemCount >= 1) {
      pid = static_cast<long>(*reinterpret_cast<unsigned long*>(data));
    }
    XFree(data);
  }
  return pid;
}

// Depth-first walk of the X11 window tree rooted at window, returning the
// first window for which match() returns true, or None.
Window FindWindowRecursive(Display* display, Window window, const std::function<bool(Window)>& match)
{
  if (match(window)) {
    return window;
  }

  Window root, parent;
  Window* children = nullptr;
  unsigned int childCount = 0;
  if (!XQueryTree(display, window, &root, &parent, &children, &childCount)) {
    return None;
  }

  Window found = None;
  for (unsigned int i = 0; i < childCount; ++i) {
    found = FindWindowRecursive(display, children[i], match);
    if (found != None) {
      break;
    }
  }
  if (children) {
    XFree(children);
  }
  return found;
}

void* PollForWindow(
    const std::function<bool(Display*, Window)>& match, std::chrono::milliseconds timeout, const char* variant
)
{
  Display* display = XOpenDisplay(nullptr);
  if (!display) {
    TEST_LOG_WARN("WaitForWindow{}: unable to open X display (is $DISPLAY set?)", variant);
    return nullptr;
  }

  const Window root = DefaultRootWindow(display);
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  for (;;) {
    const Window found = FindWindowRecursive(display, root, [&](Window w) { return match(display, w); });
    if (found != None) {
      XCloseDisplay(display);
      return reinterpret_cast<void*>(static_cast<uintptr_t>(found));
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
    std::this_thread::sleep_for(POLL_INTERVAL);
  }

  XCloseDisplay(display);
  TEST_LOG_WARN(
      "WaitForWindow{}: no matching window appeared within the timeout. If "
      "the target app uses a Wayland-native toolkit (SDL3, GTK4, Qt6 default "
      "on a Wayland session), it never becomes an X11 window at all - try "
      "launching it with SDL_VIDEODRIVER=x11 / GDK_BACKEND=x11 / "
      "QT_QPA_PLATFORM=xcb set first (see "
      "docs/cimmerian_live_app_visual_testing_gap.md).",
      variant
  );
  return nullptr;
}

} // namespace

void* WaitForWindowByTitle(const std::string& title, std::chrono::milliseconds timeout)
{
  return PollForWindow(
      [&title](Display* display, Window window) {
        return GetWindowTitle(display, window).find(title) != std::string::npos;
      },
      timeout, "ByTitle"
  );
}

void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout)
{
  return PollForWindow(
      [pid](Display* display, Window window) { return GetWindowPid(display, window) == pid; }, timeout, "ByPid"
  );
}

} // namespace Cimmerian::Visual
