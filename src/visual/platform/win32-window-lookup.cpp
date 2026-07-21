#include "cimmerian/visual/window-lookup.hpp"
#include <windows.h>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

namespace {

constexpr std::chrono::milliseconds POLL_INTERVAL {50};

std::string GetWindowTitleUtf8(HWND hwnd)
{
  const int length = GetWindowTextLengthW(hwnd);
  if (length <= 0) {
    return "";
  }

  std::wstring wide(static_cast<std::size_t>(length) + 1, L'\0');
  GetWindowTextW(hwnd, wide.data(), length + 1);

  const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8Length <= 0) {
    return "";
  }
  std::string utf8(static_cast<std::size_t>(utf8Length) - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Length, nullptr, nullptr);
  return utf8;
}

// EnumWindows only visits top-level windows, which is exactly the set a
// title/pid lookup for a separately-launched app's main window cares about.
HWND FindTopLevelWindow(const std::function<bool(HWND)>& match)
{
  struct Context {
    const std::function<bool(HWND)>* match;
    HWND found = nullptr;
  } context {&match};

  EnumWindows(
      [](HWND hwnd, LPARAM lparam) -> BOOL {
        auto* ctx = reinterpret_cast<Context*>(lparam);
        if (!IsWindowVisible(hwnd)) {
          return TRUE;
        }
        if ((*ctx->match)(hwnd)) {
          ctx->found = hwnd;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&context)
  );

  return context.found;
}

void* PollForWindow(const std::function<bool(HWND)>& match, std::chrono::milliseconds timeout, const char* variant)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  for (;;) {
    if (HWND found = FindTopLevelWindow(match)) {
      return static_cast<void*>(found);
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
    std::this_thread::sleep_for(POLL_INTERVAL);
  }

  TEST_LOG_WARN("WaitForWindow{}: no matching window appeared within the timeout.", variant);
  return nullptr;
}

} // namespace

void* WaitForWindowByTitle(const std::string& title, std::chrono::milliseconds timeout)
{
  return PollForWindow(
      [&title](HWND hwnd) { return GetWindowTitleUtf8(hwnd).find(title) != std::string::npos; }, timeout, "ByTitle"
  );
}

void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout)
{
  return PollForWindow(
      [pid](HWND hwnd) {
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        return static_cast<long>(windowPid) == pid;
      },
      timeout, "ByPid"
  );
}

} // namespace Cimmerian::Visual
