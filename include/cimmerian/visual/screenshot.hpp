#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Cimmerian::Visual {

struct Screenshot {
  std::vector<uint8_t> pixels; // RGBA, row-major
  int width = 0;
  int height = 0;

  static Screenshot Capture(void* windowHandle);
  static Screenshot LoadPNG(const std::string& path);
  void SavePNG(const std::string& path) const;
};

} // namespace Cimmerian::Visual
