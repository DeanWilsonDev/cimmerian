#include "cimmerian/visual/image-diff.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace Cimmerian::Visual {

namespace {

constexpr uint8_t DIM_FACTOR_NUMERATOR = 2;
constexpr uint8_t DIM_FACTOR_DENOMINATOR = 5; // matching pixels rendered at 40% brightness

void PaintMatching(Screenshot& diffImage, std::size_t pixelIndex, const uint8_t* sourcePixel)
{
  diffImage.pixels[pixelIndex + 0] =
      static_cast<uint8_t>(sourcePixel[0] * DIM_FACTOR_NUMERATOR / DIM_FACTOR_DENOMINATOR);
  diffImage.pixels[pixelIndex + 1] =
      static_cast<uint8_t>(sourcePixel[1] * DIM_FACTOR_NUMERATOR / DIM_FACTOR_DENOMINATOR);
  diffImage.pixels[pixelIndex + 2] =
      static_cast<uint8_t>(sourcePixel[2] * DIM_FACTOR_NUMERATOR / DIM_FACTOR_DENOMINATOR);
  diffImage.pixels[pixelIndex + 3] = 255;
}

void PaintDifferent(Screenshot& diffImage, std::size_t pixelIndex)
{
  diffImage.pixels[pixelIndex + 0] = 255;
  diffImage.pixels[pixelIndex + 1] = 0;
  diffImage.pixels[pixelIndex + 2] = 0;
  diffImage.pixels[pixelIndex + 3] = 255;
}

} // namespace

DiffResult CompareScreenshots(const Screenshot& golden, const Screenshot& actual, const DiffOptions& options)
{
  DiffResult result {};

  if (golden.width != actual.width || golden.height != actual.height) {
    result.passed = false;
    result.differentPixelCount = actual.width * actual.height;
    result.differencePercent = 100.0f;
    result.diffImage = actual;
    for (std::size_t i = 0; i + 3 < result.diffImage.pixels.size(); i += 4) {
      PaintDifferent(result.diffImage, i);
    }
    return result;
  }

  const int width = golden.width;
  const int height = golden.height;
  const int totalPixels = width * height;

  Screenshot diffImage;
  diffImage.width = width;
  diffImage.height = height;
  diffImage.pixels.resize(golden.pixels.size());

  int differentPixelCount = 0;

  for (int pixelIndexInt = 0; pixelIndexInt < totalPixels; ++pixelIndexInt) {
    const std::size_t byteIndex = static_cast<std::size_t>(pixelIndexInt) * 4;

    const uint8_t* goldenPixel = &golden.pixels[byteIndex];
    const uint8_t* actualPixel = &actual.pixels[byteIndex];

    int maxChannelDiff = 0;
    for (int channel = 0; channel < 3; ++channel) {
      const int diff = std::abs(static_cast<int>(goldenPixel[channel]) - static_cast<int>(actualPixel[channel]));
      maxChannelDiff = std::max(maxChannelDiff, diff);
    }

    const float normalizedDiff = static_cast<float>(maxChannelDiff) / 255.0f;
    const bool isDifferent = normalizedDiff > options.threshold;

    if (isDifferent) {
      ++differentPixelCount;
      PaintDifferent(diffImage, byteIndex);
    }
    else {
      PaintMatching(diffImage, byteIndex, actualPixel);
    }
  }

  const float differencePercent =
      totalPixels > 0 ? (100.0f * static_cast<float>(differentPixelCount) / static_cast<float>(totalPixels)) : 0.0f;

  result.differentPixelCount = differentPixelCount;
  result.differencePercent = differencePercent;
  result.passed = differencePercent <= options.maxDifferencePercent;
  result.diffImage = std::move(diffImage);

  return result;
}

} // namespace Cimmerian::Visual
