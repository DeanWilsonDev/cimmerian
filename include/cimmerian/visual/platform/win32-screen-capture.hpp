#pragma once

#include "../i-screen-capture.hpp"

namespace Cimmerian::Visual {

// Captures via BitBlt into a compatible DC. windowHandle is an HWND cast to
// void*; nullptr captures the desktop window (the whole screen).
class Win32ScreenCapture : public IScreenCapture {
public:
  Screenshot Capture(void* windowHandle) override;
};

} // namespace Cimmerian::Visual
