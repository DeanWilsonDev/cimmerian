#pragma once
#include "test-registry.hpp"
#include "i-test-fail-handler.hpp"
#include <cstddef>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Cimmerian {

using TestDuration = std::chrono::duration<double, std::milli>;

struct TestCaseTimingResult {
  std::string groupName;
  std::string testName;
  TestDuration elapsedTime;
};

struct TestRunSummary {
  int total = 0; // includes skipped tests
  int passed = 0;
  int failed = 0;
  int skipped = 0; // *_SKIP'd, or left out by an *_ONLY elsewhere
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

class TestRunner : public ITestFailHandler {
public:
  TestRunner();
  ~TestRunner() = default;

  void OnTestFail(const TestFailRecord& failure) override;

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

  // Runs one stage of the current test - a hook or the body - reporting an
  // exception that escapes it as a failure of that test, so one bad test can't
  // end the run. Returns false if the stage threw. A memory fault isn't an
  // exception and still takes the run down.
  template <typename TStage>
  bool RunContained(const char* stageName, TStage&& stage)
  {
    try {
      stage();
      return true;
    }
    catch (const std::exception& exception) {
      this->OnTestFail(
          {.file = "",
           .line = 0,
           .message = std::string(stageName) + " threw an exception:",
           .details = SplitLines(exception.what())}
      );
    }
    catch (...) {
      this->OnTestFail(
          {.file = "",
           .line = 0,
           .message = std::string(stageName) + " threw something that isn't a std::exception",
           .details = {}}
      );
    }
    return false;
  }

  // Runs callable and returns the first failure it triggered, or std::nullopt
  // if it completed without any assertion failure. Captured failures are
  // removed so the calling test is unaffected.
  template <typename TCallable>
  std::optional<TestFailRecord> CaptureFailure(TCallable&& callable)
  {
    const bool        priorIsFailure     = this->isFailure;
    const int         priorTotalFailures = this->totalFailures;
    const std::size_t priorPendingCount  = this->pendingFailures.size();

    callable();

    if (this->pendingFailures.size() == priorPendingCount) {
      return std::nullopt;
    }

    TestFailRecord capturedFailure = std::move(this->pendingFailures[priorPendingCount]);
    this->pendingFailures.resize(priorPendingCount);
    this->isFailure     = priorIsFailure;
    this->totalFailures = priorTotalFailures;
    return capturedFailure;
  }

  // As CaptureFailure, with the failure's message and detail lines joined by
  // newlines.
  template <typename TCallable>
  std::optional<std::string> CaptureFailureMessage(TCallable&& callable)
  {
    std::optional<TestFailRecord> capturedFailure = this->CaptureFailure(callable);
    if (!capturedFailure) {
      return std::nullopt;
    }

    std::string capturedMessage = capturedFailure->message;
    for (const std::string& detailLine : capturedFailure->details) {
      capturedMessage += "\n" + detailLine;
    }
    return capturedMessage;
  }

  // Runs callable and returns true if it triggered at least one assertion
  // failure, removing the failures so the calling test is unaffected.
  template <typename TCallable>
  bool ExpectFailure(TCallable&& callable)
  {
    return this->CaptureFailure(callable).has_value();
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
