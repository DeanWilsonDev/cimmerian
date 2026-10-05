#pragma once

#include <string_view>

namespace Cimmerian {

// How a test was registered: plainly, via a *_SKIP macro, or via a *_ONLY
// macro. Shared by unit and visual tests.
enum class TestMode {
  Normal,
  Skip,
  Only,
};

// A test's effective mode combines its own with that of the DESCRIBE it is
// nested in. Skip always wins, so an _ONLY nested inside a _SKIP'd group
// stays skipped rather than focusing the run.
constexpr TestMode CombineTestModes(TestMode scopeMode, TestMode ownMode)
{
  if (scopeMode == TestMode::Skip || ownMode == TestMode::Skip) {
    return TestMode::Skip;
  }
  if (scopeMode == TestMode::Only || ownMode == TestMode::Only) {
    return TestMode::Only;
  }
  return TestMode::Normal;
}

// Once anything is focused, only focused tests run.
constexpr bool ShouldRunTest(TestMode effectiveMode, bool hasFocusedTests)
{
  if (effectiveMode == TestMode::Skip) {
    return false;
  }
  return !hasFocusedTests || effectiveMode == TestMode::Only;
}

// --forbid-only / --forbid-skip turn the markers into errors, for CI runs
// that must not quietly drop part of the suite.
constexpr bool IsForbiddenMode(TestMode effectiveMode, bool forbidOnly, bool forbidSkip)
{
  return (forbidOnly && effectiveMode == TestMode::Only) ||
         (forbidSkip && effectiveMode == TestMode::Skip);
}

constexpr const char* ForbiddenModeReason(TestMode effectiveMode)
{
  return effectiveMode == TestMode::Only ? "_ONLY is forbidden by --forbid-only"
                                         : "_SKIP is forbidden by --forbid-skip";
}

// Process-wide rather than per-registry so a single _ONLY anywhere in the
// binary - unit or visual - focuses the whole run, the same way mocha's
// .only does across files.
class TestModeRegistry {
public:
  static TestModeRegistry& GetInstance()
  {
    static TestModeRegistry singletonInstance;
    return singletonInstance;
  }

  void RecordRegistration(TestMode effectiveMode)
  {
    if (effectiveMode == TestMode::Only) {
      this->hasFocusedTests = true;
    }
  }

  // Under --forbid-only an _ONLY is an error rather than a focus, so it no
  // longer narrows the run.
  bool HasFocusedTests() const { return this->hasFocusedTests && !this->forbidOnly; }
  bool ShouldRun(TestMode effectiveMode) const
  {
    return ShouldRunTest(effectiveMode, this->HasFocusedTests());
  }

  void SetForbidOnly(bool forbid) { this->forbidOnly = forbid; }
  void SetForbidSkip(bool forbid) { this->forbidSkip = forbid; }
  bool IsForbidden(TestMode effectiveMode) const
  {
    return IsForbiddenMode(effectiveMode, this->forbidOnly, this->forbidSkip);
  }

  // Parses --forbid-only and --forbid-skip out of argv. Other arguments are
  // ignored.
  void ParseArgs(int argc, char* argv[])
  {
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "--forbid-only") {
        this->forbidOnly = true;
      }
      else if (arg == "--forbid-skip") {
        this->forbidSkip = true;
      }
    }
  }

  TestModeRegistry(const TestModeRegistry&) = delete;
  TestModeRegistry& operator=(const TestModeRegistry&) = delete;

private:
  TestModeRegistry() = default;
  bool hasFocusedTests = false;
  bool forbidOnly = false;
  bool forbidSkip = false;
};

} // namespace Cimmerian
