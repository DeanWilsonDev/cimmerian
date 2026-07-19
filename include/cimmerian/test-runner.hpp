#pragma once
#include "test-registry.hpp"
#include "i-test-fail-handler.hpp"
#include <cstddef>
#include <chrono>
#include <string>

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

private:
  bool inTest;
  const char* currentGroup;
  const char* currentTest;
  std::string currentGroupPath;
  bool isFailure;
  int totalFailures;

  static inline TestRunner* activeInstance = nullptr;
};

} // namespace Cimmerian
