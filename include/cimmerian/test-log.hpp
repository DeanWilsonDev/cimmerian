#pragma once
#include <format>
#include <source_location>
#include <cstdio>
#include <cstdarg>
#include <string_view>
#include <array>
#include "cimmerian/ansi-codes.hpp"
#include "test-macro-helpers.hpp"

namespace Cimmerian::Log {

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

inline bool EnableVtMode()
{
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE)
    return false;

  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode))
    return false;

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  return SetConsoleMode(hOut, dwMode) != 0;
}

inline const bool g_VtEnabled = EnableVtMode();
#else
inline const bool g_VtEnabled = true;
#endif

enum class LogColor : unsigned {
  Reset = 0,
  Red = 1,
  Green = 2,
  Yellow = 3,
  Blue = 4,
  Magenta = 5,
  Cyan = 6,
  White = 7,
};

constexpr std::array<std::string_view, 8> g_ansiCodeLookupTable = {
    Ansi::ANSI_RESET,
    Ansi::ANSI_COLOR_BRIGHT_RED,
    Ansi::ANSI_COLOR_BRIGHT_GREEN,
    Ansi::ANSI_COLOR_BRIGHT_YELLOW,
    Ansi::ANSI_COLOR_BRIGHT_BLUE,
    Ansi::ANSI_COLOR_BRIGHT_MAGENTA,
    Ansi::ANSI_COLOR_BRIGHT_CYAN,
    Ansi::ANSI_COLOR_BRIGHT_WHITE,
};

constexpr std::string_view GetAnsiCode(LogColor color)
{
  return g_ansiCodeLookupTable[static_cast<std::size_t>(color)];
}

struct TestLogPrintLocation {
  std::source_location location;

  // Implicitly constructible from source_location::current()
  // This is the key trick — the default is evaluated at the CALL SITE
  TestLogPrintLocation(
      std::source_location capturedSourceLocation = std::source_location::current()
  )
      : location(capturedSourceLocation)
  {
  }
};

template <typename... Args>
CIMMERIAN_MAYBE_UNUSED inline void
TestLogPrint(LogColor color, std::format_string<Args...> formatString, Args&&... args)
{
  auto formattedMessage = std::format(formatString, std::forward<Args>(args)...);
  if (color != LogColor::Reset) {
    std::fputs(GetAnsiCode(color).data(), stdout);
  }

  std::fputs(formattedMessage.c_str(), stdout);
  std::fputc('\n', stdout);
  if (color != LogColor::Reset) {
    std::fputs(GetAnsiCode(LogColor::Reset).data(), stdout);
  }
}

} // namespace Cimmerian::Log

#define TEST_LOG_PRINT(color, fmt, ...)                                                            \
  do {                                                                                             \
    Cimmerian::Log::TestLogPrint(color, fmt __VA_OPT__(, ) __VA_ARGS__);                           \
  } while (0)

#define TAG_INFO "[INFO] "
#define TAG_WARN "[WARNING] "
#define TAG_ERROR "[ERROR] "
#define TAG_DEBUG "[DEBUG] "

#define TEST_LOG_INFO(fmt, ...)                                                                    \
  TEST_LOG_PRINT(::Cimmerian::Log::LogColor::Default, TAG_INFO fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_WARN(fmt, ...)                                                                    \
  TEST_LOG_PRINT(::Cimmerian::Log::LogColor::Yellow, TAG_WARN fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_ERROR(fmt, ...)                                                                   \
  TEST_LOG_PRINT(::Cimmerian::Log::LogColor::Red, TAG_ERROR fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_DEBUG(fmt, ...)                                                                   \
  TEST_LOG_PRINT(::Cimmerian::Log::LogColor::Magenta, TAG_DEBUG fmt __VA_OPT__(, ) __VA_ARGS__)
