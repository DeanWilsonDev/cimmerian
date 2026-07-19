#pragma once

#include "../i-screen-capture.hpp"

namespace Cimmerian::Visual {

// Captures via XGetImage/XShmGetImage. windowHandle is an X11 Window id cast
// to void*; nullptr captures the default root window (the whole screen).
class X11ScreenCapture : public IScreenCapture {
public:
  Screenshot Capture(void* windowHandle) override;
};

} // namespace Cimmerian::Visual
