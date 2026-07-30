#pragma once

#include <cstdint>
#include <string>

// Scratch windows used only by cimmerian's own visual regression self-tests
// (test/visual.test.cpp) as something real to screenshot, without touching
// the developer's actual desktop windows or requiring a consumer app.
// Implemented per-platform (scratch-window-{x11,macos}.{cpp,mm}), selected
// by the same CIMMERIAN_VISUAL_PLATFORM build the rest of visual/ uses.
namespace CimmerianTest {

// Lazily creates (once) a persistent scratch window and returns its native
// handle in the form IScreenCapture expects (X11 Window, CGWindowID, HWND -
// cast to void*).
void* GetScratchWindowHandle();

// Changes the persistent scratch window's background - "red"/"blue" pick a
// fixed color, anything else resets to the default background. Stands in
// for the consumer's own navigation-driver target.
void NavigateScratchWindow(const std::string& screenKey);

// A short-lived window mounted/unmounted per test, independent of the
// persistent scratch window above - stands in for the consumer's own
// MountComponent<T> (see docs/cimmerian_navigation_without_platform_input_proposal.md
// Proposal B).
struct ScratchComponentHost {
  void* nativeHandle = nullptr; // opaque to callers; owning platform impl's own bookkeeping
  void* captureHandle = nullptr; // what IScreenCapture::Capture() expects

  void* WindowHandle() const { return captureHandle; }
};

ScratchComponentHost MountScratchComponent(std::uint32_t backgroundColorRGB);
void UnmountScratchComponent(const ScratchComponentHost& host);

} // namespace CimmerianTest
