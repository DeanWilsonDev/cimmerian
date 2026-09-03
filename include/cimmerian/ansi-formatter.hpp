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
  static std::string ExpectedPrefix() { return Green("Expected"); }
  static std::string ReceivedPrefix() { return Red("Received"); }

  // Removes ANSI SGR sequences (ESC '[' ... 'm'), the only kind this class
  // emits. For callers that want the underlying text without color/style -
  // e.g. persisting a captured failure message to a plain-text snapshot
  // file - rather than the exact colored terminal output.
  static std::string StripCodes(const std::string& text)
  {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
      if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
        std::size_t end = i + 2;
        while (end < text.size() && text[end] != 'm') {
          ++end;
        }
        i = (end < text.size()) ? end + 1 : end;
      }
      else {
        out += text[i];
        ++i;
      }
    }
    return out;
  }
};

} // namespace Cimmerian::Ansi
