#pragma once

#include <chrono>
#include <string>

namespace Cimmerian::Visual {

// Polls the X11 window tree until a window whose title (via _NET_WM_NAME,
// falling back to WM_NAME) contains title as a substring appears, or timeout
// elapses. Returns its Window id cast to void* - the same handle convention
// VISUAL_DESCRIBE and X11ScreenCapture::Capture() expect - or nullptr on
// timeout. Logs a TEST_LOG_WARN pointing at the Wayland-native-surface
// explanation (docs/cimmerian_live_app_visual_testing_gap.md gap 1) if
// nothing ever matched, since a window that never becomes an X11 window is
// indistinguishable from one that just hasn't mapped yet.
void* WaitForWindowByTitle(const std::string& title, std::chrono::milliseconds timeout);

// Same as WaitForWindowByTitle, but matched by owning process id (read from
// the window's _NET_WM_PID property) instead of title - useful when the
// subprocess's window title isn't fixed/known ahead of time.
void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout);

} // namespace Cimmerian::Visual
