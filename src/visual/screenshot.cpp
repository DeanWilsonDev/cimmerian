#include "cimmerian/visual/screenshot.hpp"
#include "cimmerian/visual/i-screen-capture.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdexcept>

namespace Cimmerian::Visual {

Screenshot Screenshot::Capture(void* windowHandle)
{
  IScreenCapture* capture = ActiveScreenCapture::GetInstance().Get();
  if (!capture) {
    throw std::runtime_error(
        "Screenshot::Capture: no IScreenCapture registered "
        "(call VisualTestRunner::SetCapture first)"
    );
  }
  return capture->Capture(windowHandle);
}

Screenshot Screenshot::LoadPNG(const std::string& path)
{
  int width = 0;
  int height = 0;
  int sourceChannels = 0;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &sourceChannels, 4);
  if (!data) {
    throw std::runtime_error(
        "Screenshot::LoadPNG: failed to load '" + path + "': " + stbi_failure_reason()
    );
  }

  Screenshot shot;
  shot.width = width;
  shot.height = height;
  shot.pixels.assign(data, data + static_cast<std::size_t>(width) * height * 4);
  stbi_image_free(data);
  return shot;
}

void Screenshot::SavePNG(const std::string& path) const
{
  const int strideBytes = this->width * 4;
  const int result =
      stbi_write_png(path.c_str(), this->width, this->height, 4, this->pixels.data(), strideBytes);
  if (!result) {
    throw std::runtime_error("Screenshot::SavePNG: failed to write '" + path + "'");
  }
}

} // namespace Cimmerian::Visual
