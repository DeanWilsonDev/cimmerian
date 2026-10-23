#pragma once

#include <string>
#include <string_view>

namespace Cimmerian {

// While one of these exists, a fatal signal - SIGSEGV, SIGBUS, SIGFPE,
// SIGILL, SIGTRAP or SIGABRT - first writes a line to stderr naming what was
// running, as the last SetCrashContext call put it:
//
//   [CRASH] SIGSEGV (invalid memory access) in [Parser > Nested] handles commas
//
// Then the signal goes on to the handler installed before, or to the default
// action, which ends the process with that signal as it would have without
// Cimmerian. A stack overflow on the thread that made the scope is reported
// too, from a separate signal stack. RunAll keeps one for the whole run.
// Scopes can nest; the line is written once.
class CrashReportScope {
public:
  CrashReportScope();
  ~CrashReportScope();

  CrashReportScope(const CrashReportScope&) = delete;
  CrashReportScope& operator=(const CrashReportScope&) = delete;

private:
  bool installed = false;
};

// What a crash now would be reported as happening in, such as
// "[Parser] handles commas, in BEFORE_EACH". Empty reports it as happening
// outside any test. The text is copied into a fixed buffer, so a very long
// name is cut short.
void SetCrashContext(std::string_view context);
std::string GetCrashContext();

} // namespace Cimmerian
