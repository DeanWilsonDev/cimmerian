#pragma once
#include "test-registry.hpp"
#include "test-timings.hpp"
#include "i-test-fail-handler.hpp"
#include <cstddef>
#include <cstdio>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Cimmerian {

struct TestSelection;

using TestDuration = std::chrono::duration<double, std::milli>;

struct TestCaseTimingResult {
  std::string groupName;
  std::string testName;
  TestDuration elapsedTime;
};

// A group's BEFORE_ALL or AFTER_ALL that failed an assertion or threw.
struct GroupHookFailure {
  std::string groupPath; // "Group > Subgroup"
  std::string hookName;  // "BEFORE_ALL" or "AFTER_ALL"
  int testsNotRun = 0;   // for a BEFORE_ALL, the tests it failed without running
};

struct TestRunSummary {
  int total = 0; // includes skipped tests
  int passed = 0;
  // Failed tests, plus each failed AFTER_ALL and any run-level error (a bad
  // command line, a forbidden _ONLY or _SKIP), so it's nonzero whenever the
  // run failed and a main can exit on it.
  int failed = 0;
  int skipped = 0; // *_SKIP'd, or left out by an *_ONLY elsewhere
  // Every group hook that failed, in run order. A failed BEFORE_ALL reaches
  // `failed` through the tests it failed; a failed AFTER_ALL fails no test, so
  // it's counted in `failed` itself.
  std::vector<GroupHookFailure> hookFailures;
  TestDuration totalElapsedTime {0};
  TestDuration slowestTestElapsedTime {0};
  std::string slowestTestGroupName;
  std::string slowestTestName;
  std::vector<TestCaseTimingResult> perTestTimings;
  // Each top-level group's time, hooks included, in run order, as
  // --save-timings writes it. The tests outside every DESCRIBE are the group
  // named "".
  std::vector<GroupTiming> groupTimings;

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
  // Hands failures back to the runner that was active before this one, so a
  // runner made inside a test (as Cimmerian's own tests do) can come and go.
  ~TestRunner() override;

  // The fail handler registry holds this runner's address.
  TestRunner(const TestRunner&) = delete;
  TestRunner& operator=(const TestRunner&) = delete;

  // Where the run's report goes: stdout unless set.
  void SetOutput(std::FILE* stream) { this->output = stream; }

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

  // Runs one stage of the run - a test's body or one of its group's hooks -
  // reporting an exception that escapes it as a failure of that stage, so one
  // bad test can't end the run. Returns false if the stage threw. A memory
  // fault isn't an exception and still takes the run down.
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

  // Set by RunAll from --filter and --shard. A test outside it isn't run,
  // printed or counted. Until RunAll sets it, every test is in scope, so
  // RunGroup and RunOne called directly run everything.
  bool hasScope = false;
  std::unordered_set<const TestCase*> testsInScope;

  std::vector<TestFailRecord> pendingFailures;
  static inline TestRunner* activeInstance = nullptr;

  std::FILE* output = stdout;
  TestRunner* previousActive = nullptr;
  ITestFailHandler* previousHandler = nullptr;
  std::string previousCrashContext;

  // The group whose BEFORE_ALL failed, while its tests are being failed
  // without running; nullptr otherwise.
  const TestGroup* failedSetupGroup = nullptr;

  // For the summary: how --shard balanced this run when given --timings, and
  // what --save-timings did.
  std::string shardBalance;
  std::string savedTimings;
  bool savedTimingsFailed = false;

  bool IsInScope(const TestCase& test) const;
  bool WillRun(const TestCase& test) const;
  bool WillPrint(const TestCase& test) const;

  // Starts one stage: test's body when hookName is null, otherwise the named
  // hook, for test or, with no test, for the whole group. Sets what a crash
  // in it is reported as.
  void EnterStage(const TestGroup* group, const TestCase* test, const char* hookName);

  // Runs a group's BEFORE_ALL or AFTER_ALL and reports any failure under its
  // own [FAIL] line. Returns false if the hook failed.
  bool RunGroupHook(
      const TestGroup* group,
      const char* hookName,
      void (TestGroup::*hook)() const,
      TestRunSummary* summary
  );
  void PrintFailures();
  void PrintSummary(const TestRunSummary& summary, const TestSelection& selection);
};

} // namespace Cimmerian
