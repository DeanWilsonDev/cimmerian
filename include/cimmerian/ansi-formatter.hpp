#pragma once
#include <string>
#include "ansi-text-builder.hpp"

namespace Cimmerian::Ansi {

class AnsiFormatter {
public:
  static std::string Green(const std::string& text) { return AnsiTextBuilder::AsGreen(text); }

  static std::string Red(const std::string& text) { return AnsiTextBuilder::AsRed(text); }

  // Bright underline — for differing elements that exist in both
  static std::string DiffExpected(const std::string& text)
  {
    return AnsiTextBuilder::AsBrightGreen(text);
  }

  static std::string DiffReceived(const std::string& text)
  {
    return AnsiTextBuilder::AsBrightRed(text);
  }

  // Strikethrough — for extra elements in received that should not exist
  static std::string DiffExtra(const std::string& text)
  {
    return AnsiTextBuilder().BrightRed().Underline().Strikethrough().Build(text);
  }

  // Missing slot — for positions expected has but received does not
  static std::string DiffMissing(std::size_t missingCharacterCount = 1)
  {
    static constexpr const char* UTF8_EMPTY_SET_SYMBOL = "\xE2\x88\x85";

    std::string emptySetPlaceholders;
    emptySetPlaceholders.reserve(missingCharacterCount * 3);

    for (std::size_t i = 0; i < missingCharacterCount; ++i) {
      emptySetPlaceholders += UTF8_EMPTY_SET_SYMBOL;
    }

    return AnsiTextBuilder().BrightRed().Build(emptySetPlaceholders);
  }

  // Prefix symbols
  static std::string ExpectedPrefix() { return Green("+"); }
  static std::string ReceivedPrefix() { return Red("-"); }
};

} // namespace Cimmerian::Ansi
