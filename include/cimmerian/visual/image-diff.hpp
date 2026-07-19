#pragma once

#include "screenshot.hpp"

namespace Cimmerian::Visual {

struct DiffResult {
  bool passed;
  int differentPixelCount;
  float differencePercent; // 0.0-100.0
  Screenshot diffImage;    // pixels highlighted where images differ
};

struct DiffOptions {
  float threshold = 0.1f;             // max allowed per-channel diff (0-1)
  float maxDifferencePercent = 0.0f;  // fail if more than this % of pixels differ
};

DiffResult CompareScreenshots(
    const Screenshot& golden,
    const Screenshot& actual,
    const DiffOptions& options = {}
);

} // namespace Cimmerian::Visual
