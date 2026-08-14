#include "cimmerian/visual/platform/macos-screen-capture.hpp"
#include "cimmerian/test-log.hpp"
#include <ApplicationServices/ApplicationServices.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

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

// The target window's frame can still be settling (e.g. right after window
// creation or a state transition mid-test) when captured. Re-fetching once
// isn't enough to know whether the read landed mid-settle, so poll until two
// consecutive frame reads agree - the same "agreement is the real signal"
// shape as X11ScreenCapture's pixel-stability poll, but comparing the
// window's *size* here instead of pixel content, since ScreenCaptureKit
// reads already-composited pixels synchronously (no compositor-flip race)
// and the thing that can go stale instead is the frame this function uses
// to size the capture request.
struct ResolvedWindowTarget {
  SCContentFilter* filter;
  CGRect frame;
};

ResolvedWindowTarget ResolveStableWindowTarget(CGWindowID windowID)
{
  constexpr int kMaxAttempts = 8;
  constexpr auto kPollInterval = std::chrono::milliseconds(16);

  ResolvedWindowTarget current{nil, CGRectZero};
  bool haveCurrent = false;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    SCShareableContent* content = FetchShareableContentSync();
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

    ResolvedWindowTarget next{[[SCContentFilter alloc] initWithDesktopIndependentWindow:targetWindow],
                               targetWindow.frame};

    if (haveCurrent && CGRectEqualToRect(next.frame, current.frame)) {
      return next;
    }
    current = next;
    haveCurrent = true;

    if (attempt + 1 < kMaxAttempts) {
      std::this_thread::sleep_for(kPollInterval);
    }
  }

  TEST_LOG_WARN("MacOSScreenCapture: window frame did not stabilize after polling - using the last-read "
                "frame (capture may be a few pixels off if the window is still resizing/settling)");
  return current;
}

SCStreamConfiguration* MakeConfig(SCContentFilter* filter, CGRect frame)
{
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  const float pixelScale = filter.pointPixelScale > 0 ? filter.pointPixelScale : 1.0F;
  config.width = static_cast<size_t>(frame.size.width * pixelScale);
  config.height = static_cast<size_t>(frame.size.height * pixelScale);
  config.showsCursor = NO;
  config.captureResolution = SCCaptureResolutionBest;
  config.ignoreShadowsSingleWindow = YES;
  return config;
}

// ResolveStableWindowTarget only confirms the window's frame was stable up
// to its *last* read - there's still a gap between that read and the moment
// CaptureImageSync's async completion handler actually fires, in which the
// window can change again. Confirmed empirically (pharos-proto repro, 2026-
// 08-14 follow-up): ResolveStableWindowTarget's own "did not stabilize"
// warning never fired across ~30 verification runs, yet captures still came
// back a few pixels off on one axis - i.e. it kept finding two agreeing
// pre-capture reads, then losing the race after that. Close the gap by
// checking the ground truth - the captured image's actual dimensions -
// against what the resolved frame predicted, and retrying the whole
// resolve+capture cycle (not just the pre-capture poll) if they disagree.
CGImageRef CaptureWindowStable(CGWindowID windowID)
{
  constexpr int kMaxCaptureAttempts = 4;

  CGImageRef lastImage = nullptr;
  for (int attempt = 0; attempt < kMaxCaptureAttempts; ++attempt) {
    if (lastImage) {
      CGImageRelease(lastImage);
    }

    ResolvedWindowTarget resolved = ResolveStableWindowTarget(windowID);
    SCStreamConfiguration* config = MakeConfig(resolved.filter, resolved.frame);
    lastImage = CaptureImageSync(resolved.filter, config);

    if (CGImageGetWidth(lastImage) == config.width && CGImageGetHeight(lastImage) == config.height) {
      return lastImage;
    }
  }

  TEST_LOG_WARN("MacOSScreenCapture: captured image size still didn't match the last confirmed-stable "
                "window frame after {} full resolve+capture attempts - window may still be resizing; "
                "returning the last capture anyway",
                kMaxCaptureAttempts);
  return lastImage;
}

} // namespace

Screenshot MacOSScreenCapture::Capture(void* windowHandle)
{
  @autoreleasepool {
    const CGWindowID windowID =
        windowHandle ? static_cast<CGWindowID>(reinterpret_cast<uintptr_t>(windowHandle)) : kCGNullWindowID;

    CGImageRef image = nullptr;

    if (windowID != kCGNullWindowID) {
      image = CaptureWindowStable(windowID);
    }
    else {
      SCShareableContent* content = FetchShareableContentSync();
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
      SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:mainDisplay excludingWindows:@[]];
      SCStreamConfiguration* config = MakeConfig(filter, mainDisplay.frame);
      image = CaptureImageSync(filter, config);
    }

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
