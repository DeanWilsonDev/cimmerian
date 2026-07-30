#include "cimmerian/visual/platform/macos-screen-capture.hpp"
#include <ApplicationServices/ApplicationServices.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Cimmerian::Visual {

namespace {

// CGWindowListCreateImage/CGDisplayCreateImage were obsoleted in the macOS
// 15 SDK in favor of ScreenCaptureKit. SCShareableContent enumeration and
// SCScreenshotManager's completion handlers are async-only, so this bridges
// them to a synchronous call via dispatch semaphores - the surrounding
// IScreenCapture interface is synchronous and callers (VisualTestRunner)
// aren't set up to await a future.
SCShareableContent* FetchShareableContentSync()
{
  __block SCShareableContent* result = nil;
  __block NSError* fetchError = nil;
  dispatch_semaphore_t sema = dispatch_semaphore_create(0);

  [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                              onScreenWindowsOnly:YES
                                                completionHandler:^(SCShareableContent* content, NSError* error) {
                                                  result = content;
                                                  fetchError = error;
                                                  dispatch_semaphore_signal(sema);
                                                }];
  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

  if (!result) {
    std::string message = "MacOSScreenCapture: failed to enumerate shareable content";
    if (fetchError) {
      message += ": " + std::string(fetchError.localizedDescription.UTF8String);
    }
    throw std::runtime_error(message);
  }
  return result;
}

CGImageRef CaptureImageSync(SCContentFilter* filter, SCStreamConfiguration* config)
{
  __block CGImageRef result = nullptr;
  __block NSError* captureError = nil;
  dispatch_semaphore_t sema = dispatch_semaphore_create(0);

  [SCScreenshotManager captureImageWithFilter:filter
                                 configuration:config
                             completionHandler:^(CGImageRef image, NSError* error) {
                               if (image) {
                                 result = CGImageRetain(image);
                               }
                               captureError = error;
                               dispatch_semaphore_signal(sema);
                             }];
  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

  if (!result) {
    std::string message = "MacOSScreenCapture: failed to capture window/screen image";
    if (captureError) {
      message += ": " + std::string(captureError.localizedDescription.UTF8String);
    }
    throw std::runtime_error(message);
  }
  return result;
}

} // namespace

Screenshot MacOSScreenCapture::Capture(void* windowHandle)
{
  @autoreleasepool {
    const CGWindowID windowID =
        windowHandle ? static_cast<CGWindowID>(reinterpret_cast<uintptr_t>(windowHandle)) : kCGNullWindowID;

    SCShareableContent* content = FetchShareableContentSync();

    SCContentFilter* filter = nil;
    CGRect frame = CGRectZero;

    if (windowID != kCGNullWindowID) {
      SCWindow* targetWindow = nil;
      for (SCWindow* window in content.windows) {
        if (window.windowID == windowID) {
          targetWindow = window;
          break;
        }
      }
      if (!targetWindow) {
        throw std::runtime_error("MacOSScreenCapture: window not found in shareable content (Screen Recording "
                                  "permission may be missing)");
      }
      filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:targetWindow];
      frame = targetWindow.frame;
    }
    else {
      const CGDirectDisplayID mainDisplayID = CGMainDisplayID();
      SCDisplay* mainDisplay = nil;
      for (SCDisplay* display in content.displays) {
        if (display.displayID == mainDisplayID) {
          mainDisplay = display;
          break;
        }
      }
      if (!mainDisplay) {
        throw std::runtime_error("MacOSScreenCapture: main display not found in shareable content (Screen "
                                  "Recording permission may be missing)");
      }
      filter = [[SCContentFilter alloc] initWithDisplay:mainDisplay excludingWindows:@[]];
      frame = mainDisplay.frame;
    }

    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    const float pixelScale = filter.pointPixelScale > 0 ? filter.pointPixelScale : 1.0F;
    config.width = static_cast<size_t>(frame.size.width * pixelScale);
    config.height = static_cast<size_t>(frame.size.height * pixelScale);
    config.showsCursor = NO;
    config.captureResolution = SCCaptureResolutionBest;
    config.ignoreShadowsSingleWindow = YES;

    CGImageRef image = CaptureImageSync(filter, config);

    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);

    Screenshot shot;
    shot.width = static_cast<int>(width);
    shot.height = static_cast<int>(height);
    shot.pixels.resize(width * height * 4);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        shot.pixels.data(), width, height, 8, width * 4, colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context) {
      CGImageRelease(image);
      throw std::runtime_error("MacOSScreenCapture: failed to create bitmap context");
    }

    CGContextDrawImage(context, CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)), image);

    CGContextRelease(context);
    CGImageRelease(image);

    return shot;
  }
}

} // namespace Cimmerian::Visual
