#include "cimmerian/visual/platform/macos-screen-capture.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <cstdint>
#include <stdexcept>

namespace Cimmerian::Visual {

Screenshot MacOSScreenCapture::Capture(void* windowHandle)
{
  const CGWindowID windowID =
      windowHandle ? static_cast<CGWindowID>(reinterpret_cast<uintptr_t>(windowHandle)) : kCGNullWindowID;

  CGImageRef image = nullptr;
  if (windowID != kCGNullWindowID) {
    image =
        CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, kCGWindowImageDefault);
  }
  else {
    image = CGDisplayCreateImage(CGMainDisplayID());
  }

  if (!image) {
    throw std::runtime_error("MacOSScreenCapture: failed to capture window/screen image");
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

} // namespace Cimmerian::Visual
