#pragma once
#include <string>

namespace Umbra::Test {

class AnsiFormatter {
public:
  // Highlight codes — bright + underline for diffs
  static constexpr const char* ResetCode = "\033[0m";
  static constexpr const char* GreenCode = "\033[32m";
  static constexpr const char* RedCode = "\033[31m";
  static constexpr const char* BrightGreenCode = "\033[92;4m"; // bright green + underline
  static constexpr const char* BrightRedCode = "\033[91;4m";   // bright red + underline
  static constexpr const char* BrightRedStrikeCode =
      "\033[91;4;9m"; // bright red + underline + strikethrough

  static std::string Green(const std::string& text)
  {
    return std::string(GreenCode) + text + ResetCode;
  }

  static std::string Red(const std::string& text)
  {
    return std::string(RedCode) + text + ResetCode;
  }

  // Bright underline — for differing elements that exist in both
  static std::string DiffExpected(const std::string& text)
  {
    return std::string(BrightGreenCode) + text + ResetCode;
  }

  static std::string DiffReceived(const std::string& text)
  {
    return std::string(BrightRedCode) + text + ResetCode;
  }

  // Strikethrough — for extra elements in received that should not exist
  static std::string DiffExtra(const std::string& text)
  {
    return std::string(BrightRedStrikeCode) + text + ResetCode;
  }

  // Missing slot — for positions expected has but received does not
  static std::string DiffMissing(std::size_t missingCharCount = 1)
  {
    std::string missingSlots(missingCharCount, ' ');
    for (std::size_t i = 0; i < missingCharCount; ++i) {
      missingSlots[i] = '\xE2'; // UTF-8 prefix handled below
    }
    // Build N ∅ symbols
    std::string result(BrightRedCode);
    for (std::size_t i = 0; i < missingCharCount; ++i) {
      result += "\xE2\x88\x85"; // UTF-8 encoding of ∅
    }
    result += ResetCode;
    return result;
  }

  // Prefix symbols
  static std::string ExpectedPrefix() { return Green("+"); }
  static std::string ReceivedPrefix() { return Red("-"); }
};

} // namespace Umbra::Test
