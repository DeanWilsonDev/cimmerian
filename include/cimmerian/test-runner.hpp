#pragma once
#include "test-registry.hpp"
#include "i-test-fail-handler.hpp"
#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace Cimmerian {

using TestDuration = std::chrono::duration<double, std::milli>;

struct TestCaseTimingResult {
  std::string groupName;
  std::string testName;
  TestDuration elapsedTime;
};

struct TestRunSummary {
  int total = 0;
  int passed = 0;
  int failed = 0;
  TestDuration totalElapsedTime {0};
  TestDuration slowestTestElapsedTime {0};
  std::string slowestTestGroupName;
  std::string slowestTestName;
  std::vector<TestCaseTimingResult> perTestTimings;

  // Snapshot testing extension (string / inline / hash snapshots)
  int snapshotsMatched = 0;
  int snapshotsFailed = 0;
  int snapshotsUpdated = 0;
  int snapshotsMissing = 0;
  int inlineRewriteCount = 0;
};

struct TestFailRecord {
  std::string file;
  int line;
  std::string message;
};

class TestRunner : public ITestFailHandler {
public:
  TestRunner();
  ~TestRunner() = default;

  void OnTestFail(const char* file, int line, const char* msg) override;

  void RunOne(const TestGroup* group, const TestCase* test, TestRunSummary* summary);
  TestRunSummary* RunGroup(const TestGroup* group, TestRunSummary* summary);
  TestRunSummary RunAll(const TestRegistry* registry);

  void BeginContext(const char* groupName, const char* testName);
  void EndContext();

  bool IsInTest() const { return this->inTest; }
  const char* GetCurrentGroup() const { return this->currentGroup; }
  const char* GetCurrentTest() const { return this->currentTest; }
  const std::string& GetCurrentGroupPath() const { return this->currentGroupPath; }
  bool IsFailure() const { return this->isFailure; }
  int GetTotalFailures() const { return this->totalFailures; }

  // The most recently constructed TestRunner. Lets extensions (snapshot
  // macros, visual macros) reach the running test's context without every
  // extension needing its own registry of the active runner.
  static TestRunner* GetActive() { return activeInstance; }

  // Runs callable and returns true if it triggered at least one assertion failure.
  // On success the captured failures are removed, leaving the calling test unaffected.

  // Runs callable and returns the failure message if one was triggered, or
  // std::nullopt if the callable completed without any assertion failure.
  // On a captured failure the record is removed so the calling test is unaffected.
  template <typename TCallable>
  std::optional<std::string> CaptureFailureMessage(TCallable&& callable)
  {
    const bool        priorIsFailure     = this->isFailure;
    const int         priorTotalFailures = this->totalFailures;
    const std::size_t priorPendingCount  = this->pendingFailures.size();

    callable();

    const bool newFailureOccurred = this->pendingFailures.size() > priorPendingCount;

    if (newFailureOccurred) {
      std::string capturedMessage = this->pendingFailures[priorPendingCount].message;
      this->pendingFailures.resize(priorPendingCount);
      this->isFailure     = priorIsFailure;
      this->totalFailures = priorTotalFailures;
      return capturedMessage;
    }

    return std::nullopt;
  }

  template <typename TCallable>
  bool ExpectFailure(TCallable&& callable)
  {
    const bool        priorIsFailure     = this->isFailure;
    const int         priorTotalFailures = this->totalFailures;
    const std::size_t priorPendingCount  = this->pendingFailures.size();

    callable();

    const bool newFailureOccurred = this->pendingFailures.size() > priorPendingCount;

    if (newFailureOccurred) {
      this->pendingFailures.resize(priorPendingCount);
      this->isFailure     = priorIsFailure;
      this->totalFailures = priorTotalFailures;
    }

    return newFailureOccurred;
  }


private:
  bool inTest;
  const char* currentGroup;
  const char* currentTest;
  std::string currentGroupPath;
  bool isFailure;
  int totalFailures;

  std::vector<TestFailRecord> pendingFailures;
  static inline TestRunner* activeInstance = nullptr;
};

} // namespace Cimmerian
