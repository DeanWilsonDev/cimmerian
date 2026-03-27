#pragma once
#include <string>
#include "ansi-codes.hpp"

namespace Cimmerian::Ansi {

class AnsiTextBuilder {
public:
  // ── Chainable color setters ──────────────────────────────────────────────

  AnsiTextBuilder& Red()
  {
    this->colorCode = ANSI_COLOR_RED;
    return *this;
  }
  AnsiTextBuilder& Green()
  {
    this->colorCode = ANSI_COLOR_GREEN;
    return *this;
  }
  AnsiTextBuilder& Yellow()
  {
    this->colorCode = ANSI_COLOR_YELLOW;
    return *this;
  }
  AnsiTextBuilder& Blue()
  {
    this->colorCode = ANSI_COLOR_BLUE;
    return *this;
  }
  AnsiTextBuilder& Magenta()
  {
    this->colorCode = ANSI_COLOR_MAGENTA;
    return *this;
  }
  AnsiTextBuilder& Cyan()
  {
    this->colorCode = ANSI_COLOR_CYAN;
    return *this;
  }
  AnsiTextBuilder& White()
  {
    this->colorCode = ANSI_COLOR_WHITE;
    return *this;
  }

  AnsiTextBuilder& BrightRed()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_RED;
    return *this;
  }
  AnsiTextBuilder& BrightGreen()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_GREEN;
    return *this;
  }
  AnsiTextBuilder& BrightYellow()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_YELLOW;
    return *this;
  }
  AnsiTextBuilder& BrightBlue()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_BLUE;
    return *this;
  }
  AnsiTextBuilder& BrightMagenta()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_MAGENTA;
    return *this;
  }
  AnsiTextBuilder& BrightCyan()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_CYAN;
    return *this;
  }
  AnsiTextBuilder& BrightWhite()
  {
    this->colorCode = ANSI_COLOR_BRIGHT_WHITE;
    return *this;
  }

  // ── Chainable decoration setters ─────────────────────────────────────────

  AnsiTextBuilder& Bold()
  {
    this->isBold = true;
    return *this;
  }
  AnsiTextBuilder& Dim()
  {
    this->isDim = true;
    return *this;
  }
  AnsiTextBuilder& Italic()
  {
    this->isItalic = true;
    return *this;
  }
  AnsiTextBuilder& Underline()
  {
    this->isUnderline = true;
    return *this;
  }
  AnsiTextBuilder& Strikethrough()
  {
    this->isStrikethrough = true;
    return *this;
  }

  // ── Terminal build ────────────────────────────────────────────────────────

  std::string Build(const std::string& textToDecorate) const
  {
    std::string builtAnsiSequence;

    if (this->colorCode)
      builtAnsiSequence += this->colorCode;

    if (this->isBold)
      builtAnsiSequence += ANSI_DECORATION_BOLD;

    if (this->isDim)
      builtAnsiSequence += ANSI_DECORATION_DIM;

    if (this->isItalic)
      builtAnsiSequence += ANSI_DECORATION_ITALIC;

    if (this->isUnderline)
      builtAnsiSequence += ANSI_DECORATION_UNDERLINE;

    if (this->isStrikethrough)
      builtAnsiSequence += ANSI_DECORATION_STRIKETHROUGH;

    builtAnsiSequence += textToDecorate;
    builtAnsiSequence += ANSI_RESET;

    return builtAnsiSequence;
  }

  // ── Convenience: static one-shot builders ────────────────────────────────
  // For the common case where you just want a color with no decoration,
  // without constructing a builder at the call site.

  static std::string AsRed(const std::string& text) { return AnsiTextBuilder().Red().Build(text); }
  static std::string AsGreen(const std::string& text)
  {
    return AnsiTextBuilder().Green().Build(text);
  }
  static std::string AsYellow(const std::string& text)
  {
    return AnsiTextBuilder().Yellow().Build(text);
  }
  static std::string AsBlue(const std::string& text)
  {
    return AnsiTextBuilder().Blue().Build(text);
  }
  static std::string AsMagenta(const std::string& text)
  {
    return AnsiTextBuilder().Magenta().Build(text);
  }
  static std::string AsCyan(const std::string& text)
  {
    return AnsiTextBuilder().Cyan().Build(text);
  }
  static std::string AsWhite(const std::string& text)
  {
    return AnsiTextBuilder().White().Build(text);
  }

  static std::string AsBrightRed(const std::string& text)
  {
    return AnsiTextBuilder().BrightRed().Build(text);
  }
  static std::string AsBrightGreen(const std::string& text)
  {
    return AnsiTextBuilder().BrightGreen().Build(text);
  }
  static std::string AsBrightYellow(const std::string& text)
  {
    return AnsiTextBuilder().BrightYellow().Build(text);
  }
  static std::string AsBrightBlue(const std::string& text)
  {
    return AnsiTextBuilder().BrightBlue().Build(text);
  }
  static std::string AsBrightMagenta(const std::string& text)
  {
    return AnsiTextBuilder().BrightMagenta().Build(text);
  }
  static std::string AsBrightCyan(const std::string& text)
  {
    return AnsiTextBuilder().BrightCyan().Build(text);
  }
  static std::string AsBrightWhite(const std::string& text)
  {
    return AnsiTextBuilder().BrightWhite().Build(text);
  }

private:
  // ── Builder state ─────────────────────────────────────────────────────────

  const char* colorCode = nullptr;
  bool isBold = false;
  bool isDim = false;
  bool isItalic = false;
  bool isUnderline = false;
  bool isStrikethrough = false;
};
} // namespace Cimmerian::Ansi
