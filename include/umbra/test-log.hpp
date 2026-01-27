#pragma once
#include <format>
#include <source_location>
#include <cstdio>
#include <cstdarg>
#include <string_view>
#include <array>
#include "test-macro-helpers.hpp"

namespace Umbra::Test::Log {

#define LOG_COLOR_CODE_RED "\x1b[91m"
#define LOG_COLOR_CODE_GREEN "\x1b[92m"
#define LOG_COLOR_CODE_YELLOW "\x1b[93m"
#define LOG_COLOR_CODE_BLUE "\x1b[94m"
#define LOG_COLOR_CODE_MAGENTA "\x1b[95m"
#define LOG_COLOR_CODE_CYAN "\x1b[96m"
#define LOG_COLOR_CODE_WHITE "\x1b[97m"
#define LOG_COLOR_CODE_DEFAULT "\x1b[0m"

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
  Default = 0,
  Red = 1,
  Green = 2,
  Yellow = 3,
  Blue = 4,
  Magenta = 5,
  Cyan = 6,
  White = 7,
};

constexpr std::array<std::string_view, 8> g_AnsiCodes = {
    LOG_COLOR_CODE_DEFAULT, LOG_COLOR_CODE_RED,     LOG_COLOR_CODE_GREEN, LOG_COLOR_CODE_YELLOW,
    LOG_COLOR_CODE_BLUE,    LOG_COLOR_CODE_MAGENTA, LOG_COLOR_CODE_CYAN,  LOG_COLOR_CODE_WHITE
};

constexpr std::string_view GetAnsiCode(LogColor color)
{
  return g_AnsiCodes[static_cast<std::size_t>(color)];
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
UMBRA_MAYBE_UNUSED inline void TestLogPrint(
    LogColor color,
    TestLogPrintLocation locationCapture,
    std::format_string<Args...> formatString,
    Args&&... args
)
{
  auto formattedMessage = std::format(formatString, std::forward<Args>(args)...);
  if (color != LogColor::Default) {
    std::fputs(GetAnsiCode(color).data(), stdout);
  }
  std::fprintf(
      stdout, "[%s:%d] ", locationCapture.location.file_name(), locationCapture.location.line()
  );
  std::fputs(formattedMessage.c_str(), stdout);
  std::fputc('\n', stdout);
  if (color != LogColor::Default) {
    std::fputs(GetAnsiCode(LogColor::Default).data(), stdout);
  }
}

} // namespace Umbra::Test::Log

#define TEST_LOG_PRINT(color, loc, fmt, ...)                                                       \
  do {                                                                                             \
    Umbra::Test::Log::TestLogPrint(color, loc, fmt __VA_OPT__(, ) __VA_ARGS__);                    \
  } while (0)

#define TAG_INFO "[INFO] "
#define TAG_WARN "[WARNING] "
#define TAG_ERROR "[ERROR] "
#define TAG_DEBUG "[DEBUG] "

#define TEST_LOG_INFO(fmt, ...)                                                                    \
  TEST_LOG_PRINT(::Umbra::Test::Log::LogColor::Default, {}, TAG_INFO fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_WARN(fmt, ...)                                                                    \
  TEST_LOG_PRINT(::Umbra::Test::Log::LogColor::Yellow, {}, TAG_WARN fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_ERROR(fmt, ...)                                                                   \
  TEST_LOG_PRINT(::Umbra::Test::Log::LogColor::Red, {}, TAG_ERROR fmt __VA_OPT__(, ) __VA_ARGS__)

#define TEST_LOG_DEBUG(fmt, ...)                                                                   \
  TEST_LOG_PRINT(::Umbra::Test::Log::LogColor::Magenta, {}, TAG_DEBUG fmt __VA_OPT__(, ) __VA_ARGS__)
