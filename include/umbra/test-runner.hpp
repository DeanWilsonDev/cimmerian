#pragma once
#include "test-registry.hpp"
#include "i-test-fail-handler.hpp"
#include <cstddef>
#include <chrono>
#include <string>

namespace Umbra::Test {

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
  bool IsFailure() const { return this->isFailure; }
  int GetTotalFailures() const { return this->totalFailures; }

private:
  bool inTest;
  const char* currentGroup;
  const char* currentTest;
  bool isFailure;
  int totalFailures;
};

} // namespace Umbra::Test
