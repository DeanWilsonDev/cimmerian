#include "cimmerian/visual/window-lookup.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

namespace {

constexpr std::chrono::milliseconds POLL_INTERVAL {50};

std::string CFStringToUtf8(CFStringRef str)
{
  if (!str) {
    return "";
  }
  const CFIndex length = CFStringGetLength(str);
  const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(static_cast<std::size_t>(maxSize), '\0');
  if (!CFStringGetCString(str, result.data(), maxSize, kCFStringEncodingUTF8)) {
    return "";
  }
  result.resize(std::char_traits<char>::length(result.c_str()));
  return result;
}

// CGWindowListCopyWindowInfo enumerates on-screen windows across the whole
// session (not just this process's own), which is exactly what's needed to
// find a separately-launched app's window.
CGWindowID FindWindow(const std::function<bool(CFDictionaryRef)>& match)
{
  CFArrayRef windowList = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID
  );
  if (!windowList) {
    return kCGNullWindowID;
  }

  CGWindowID found = kCGNullWindowID;
  const CFIndex count = CFArrayGetCount(windowList);
  for (CFIndex i = 0; i < count; ++i) {
    auto* entry = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
    if (match(entry)) {
      CFNumberRef windowIdNumber =
          static_cast<CFNumberRef>(CFDictionaryGetValue(entry, kCGWindowNumber));
      uint32_t windowId = 0;
      if (windowIdNumber && CFNumberGetValue(windowIdNumber, kCFNumberSInt32Type, &windowId)) {
        found = static_cast<CGWindowID>(windowId);
      }
      break;
    }
  }

  CFRelease(windowList);
  return found;
}

void* PollForWindow(
    const std::function<bool(CFDictionaryRef)>& match, std::chrono::milliseconds timeout, const char* variant
)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  for (;;) {
    const CGWindowID found = FindWindow(match);
    if (found != kCGNullWindowID) {
      return reinterpret_cast<void*>(static_cast<uintptr_t>(found));
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
      [&title](CFDictionaryRef entry) {
        auto* name = static_cast<CFStringRef>(CFDictionaryGetValue(entry, kCGWindowName));
        return CFStringToUtf8(name).find(title) != std::string::npos;
      },
      timeout, "ByTitle"
  );
}

void* WaitForWindowByPid(long pid, std::chrono::milliseconds timeout)
{
  return PollForWindow(
      [pid](CFDictionaryRef entry) {
        auto* pidNumber = static_cast<CFNumberRef>(CFDictionaryGetValue(entry, kCGWindowOwnerPID));
        int32_t ownerPid = 0;
        if (pidNumber && CFNumberGetValue(pidNumber, kCFNumberSInt32Type, &ownerPid)) {
          return static_cast<long>(ownerPid) == pid;
        }
        return false;
      },
      timeout, "ByPid"
  );
}

} // namespace Cimmerian::Visual
