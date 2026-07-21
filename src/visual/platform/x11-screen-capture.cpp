#include "cimmerian/visual/platform/x11-screen-capture.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdint>
#include <stdexcept>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

namespace {

// XImage channel masks can vary by depth/visual (e.g. 16-bit 5-6-5), so
// normalize each channel by its own mask width instead of assuming 8-bit
// aligned 0xFF0000/0x00FF00/0x0000FF masks.
unsigned char ExtractChannel(unsigned long pixel, unsigned long mask)
{
  if (mask == 0) {
    return 0;
  }
  int shift = 0;
  unsigned long shiftedMask = mask;
  while ((shiftedMask & 1) == 0) {
    ++shift;
    shiftedMask >>= 1;
  }
  const unsigned long value = (pixel & mask) >> shift;
  const unsigned long maxValue = shiftedMask;
  if (maxValue == 0) {
    return 0;
  }
  return static_cast<unsigned char>((value * 255) / maxValue);
}

} // namespace

Screenshot X11ScreenCapture::Capture(void* windowHandle)
{
  Display* display = XOpenDisplay(nullptr);
  if (!display) {
    throw std::runtime_error("X11ScreenCapture: unable to open X display (is $DISPLAY set?)");
  }

  const Window window =
      windowHandle ? static_cast<Window>(reinterpret_cast<uintptr_t>(windowHandle)) : DefaultRootWindow(display);

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display, window, &attributes)) {
    XCloseDisplay(display);
    TEST_LOG_WARN(
        "X11ScreenCapture: XGetWindowAttributes failed for the given window "
        "handle - if the target app uses a Wayland-native toolkit (SDL3, "
        "GTK4, Qt6 default on a Wayland session), it never becomes an X11 "
        "window at all and this handle was never valid. Try launching it "
        "with SDL_VIDEODRIVER=x11 / GDK_BACKEND=x11 / QT_QPA_PLATFORM=xcb "
        "set first (see docs/cimmerian_live_app_visual_testing_gap.md)."
    );
    throw std::runtime_error("X11ScreenCapture: XGetWindowAttributes failed for the given window handle");
  }

  XImage* image =
      XGetImage(display, window, 0, 0, attributes.width, attributes.height, AllPlanes, ZPixmap);
  if (!image) {
    XCloseDisplay(display);
    throw std::runtime_error("X11ScreenCapture: XGetImage failed");
  }

  Screenshot shot;
  shot.width = image->width;
  shot.height = image->height;
  shot.pixels.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * 4);

  for (int y = 0; y < image->height; ++y) {
    for (int x = 0; x < image->width; ++x) {
      const unsigned long pixel = XGetPixel(image, x, y);
      const std::size_t idx = (static_cast<std::size_t>(y) * image->width + static_cast<std::size_t>(x)) * 4;
      shot.pixels[idx + 0] = ExtractChannel(pixel, image->red_mask);
      shot.pixels[idx + 1] = ExtractChannel(pixel, image->green_mask);
      shot.pixels[idx + 2] = ExtractChannel(pixel, image->blue_mask);
      shot.pixels[idx + 3] = 255;
    }
  }

  XDestroyImage(image);
  XCloseDisplay(display);

  return shot;
}

} // namespace Cimmerian::Visual
