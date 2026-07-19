#pragma once

#include "../i-screen-capture.hpp"

namespace Cimmerian::Visual {

// Captures via CGWindowListCreateImage (specific window) or
// CGDisplayCreateImage (whole screen, windowHandle == nullptr).
class MacOSScreenCapture : public IScreenCapture {
public:
  Screenshot Capture(void* windowHandle) override;
};

} // namespace Cimmerian::Visual
