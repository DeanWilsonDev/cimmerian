#include "scratch-window.hpp"
#import <AppKit/AppKit.h>

namespace CimmerianTest {

namespace {

NSColor* RGBColor(std::uint32_t rgb)
{
  const CGFloat r = ((rgb >> 16) & 0xFF) / 255.0;
  const CGFloat g = ((rgb >> 8) & 0xFF) / 255.0;
  const CGFloat b = (rgb & 0xFF) / 255.0;
  return [NSColor colorWithSRGBRed:r green:g blue:b alpha:1.0];
}

// A borderless NSWindow only becomes visible to the window server - and
// therefore discoverable via SCShareableContent's onScreenWindowsOnly
// filter that MacOSScreenCapture relies on - once NSApplication exists and
// the run loop has had a moment to process the order-front request. These
// windows are snapshot-only (no UI interaction), so a full `[NSApp run]`
// event loop is unnecessary; a short manual pump after each mutation is
// enough for the window manager to register the change.
void EnsureApplicationInitialized()
{
  static bool initialized = [] {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    return true;
  }();
  (void)initialized;
}

void PumpRunLoop()
{
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
}

NSWindow* CreateWindow(int width, int height, NSColor* backgroundColor)
{
  EnsureApplicationInitialized();

  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(10, 10, width, height)
                                   styleMask:NSWindowStyleMaskBorderless
                                     backing:NSBackingStoreBuffered
                                       defer:NO];
  [window setReleasedWhenClosed:NO];
  [window setBackgroundColor:backgroundColor];
  [window orderFrontRegardless];
  PumpRunLoop();
  return window;
}

NSWindow* GetPersistentWindow()
{
  static NSWindow* window = CreateWindow(200, 150, [NSColor whiteColor]);
  return window;
}

} // namespace

void* GetScratchWindowHandle()
{
  NSWindow* window = GetPersistentWindow();
  return reinterpret_cast<void*>(static_cast<uintptr_t>(window.windowNumber));
}

void NavigateScratchWindow(const std::string& screenKey)
{
  NSWindow* window = GetPersistentWindow();
  NSColor* color = [NSColor whiteColor];
  if (screenKey == "red") {
    color = RGBColor(0xCC3333);
  }
  else if (screenKey == "blue") {
    color = RGBColor(0x3355CC);
  }
  [window setBackgroundColor:color];
  PumpRunLoop();
}

ScratchComponentHost MountScratchComponent(std::uint32_t backgroundColorRGB)
{
  NSWindow* window = CreateWindow(120, 80, RGBColor(backgroundColorRGB));

  ScratchComponentHost host;
  host.nativeHandle = (__bridge_retained void*)window; // balanced in UnmountScratchComponent
  host.captureHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(window.windowNumber));
  return host;
}

void UnmountScratchComponent(const ScratchComponentHost& host)
{
  NSWindow* window = (__bridge_transfer NSWindow*)host.nativeHandle;
  [window close];
}

} // namespace CimmerianTest
