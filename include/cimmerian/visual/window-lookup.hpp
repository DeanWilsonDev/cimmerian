#pragma once

#include <chrono>
#include <string>

namespace Cimmerian::Visual {

// Polls for a window matching title as a substring, or timeout elapses.
// Returns its native handle (X11 Window, HWND, or CGWindowID - whichever
// this platform's IScreenCapture/IEventInjector backends expect) cast to
// void*, or nullptr on timeout. On X11, title is matched via _NET_WM_NAME
// (falling back to WM_NAME); logs a TEST_LOG_WARN pointing at the
// Wayland-native-surface explanation
// (docs/cimmerian_live_app_visual_testing_gap.md gap 1) if nothing ever
// matched, since a window that never becomes an X11 window is
// indistinguishable from one that just hasn't mapped yet.
//
// Implemented per-platform (src/visual/platform/{x11,win32,macos}-window-lookup.cpp),
// selected by the same CIMMERIAN_VISUAL_PLATFORM build the rest of visual/
// uses - only one implementation is ever compiled in.
void* WaitForWindowByTitle(const std::string& title, std::chrono::milliseconds timeout);

// Same as WaitForWindowByTitle, but matched by owning process id instead of
// title - useful when the subprocess's window title isn't fixed/known ahead
// of time.
void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout);

} // namespace Cimmerian::Visual
