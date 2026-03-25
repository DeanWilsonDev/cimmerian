#include "umbra/test-runner.hpp"
#include "umbra/test-fail-handler-registry.hpp"
#include "umbra/test-group.hpp"
#include "umbra/test-log.hpp"
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>

namespace Umbra::Test {

using namespace Umbra::Test::Log;

void TestRunner::BeginContext(const char* groupName, const char* testName)
{
  this->currentGroup = groupName;
  this->currentTest = testName;
  this->inTest = true;
}

void TestRunner::EndContext()
{
  this->inTest = false;
}

TestRunner::TestRunner()
    : inTest(false)
    , currentGroup(nullptr)
    , currentTest(nullptr)
    , isFailure(false)
    , totalFailures(0)
{
  TestFailHandlerRegistry::GetInstance().RegisterHandler(this);
}

void TestRunner::OnTestFail(const char* file, int line, const char* msg)
{
  if (!this->inTest) {
    std::fprintf(
        stderr, LOG_COLOR_CODE_RED TAG_ERROR "%s:%d: test failure outside of running test: %s\n",
        file, line, msg
    );
  }

  this->isFailure = true;
  this->totalFailures++;
  TEST_LOG_PRINT(LogColor::Red, {}, "[FAIL] {}:{} {}", file, line, msg);
}

void TestRunner::RunOne(const TestGroup* group, const TestCase* test, TestRunSummary* summary)
{
  this->isFailure = false;

  this->BeginContext(group->GetName(), "(before_each)");
  group->ExecuteBeforeEach();
  EndContext();

  this->BeginContext(group->GetName(), test->GetName());
  auto startTime = std::chrono::high_resolution_clock::now();
  test->Run();
  auto endTime = std::chrono::high_resolution_clock::now();
  EndContext();

  this->BeginContext(group->GetName(), "(after_each)");
  group->ExecuteAfterEach();
  EndContext();

  TestDuration elapsedTime = endTime - startTime;

  summary->perTestTimings.push_back(
      {.groupName = group->GetName(), .testName = test->GetName(), .elapsedTime = elapsedTime}
  );

  if (elapsedTime > summary->slowestTestElapsedTime) {
    summary->slowestTestElapsedTime = elapsedTime;
    summary->slowestTestGroupName = group->GetName();
    summary->slowestTestName = test->GetName();
  }

  summary->total++;
  if (this->isFailure) {
    summary->failed++;
    TEST_LOG_PRINT(
        LogColor::Red, {}, "[FAIL] [{}] {}  ({:.4f}ms)", CheckGroupName(group->GetName()),
        test->GetName(), elapsedTime.count()
    );
  }
  else {
    summary->passed++;
    TEST_LOG_PRINT(
        LogColor::Green, {}, "[PASS] [{}] {}  ({:.4f}ms)", CheckGroupName(group->GetName()),
        test->GetName(), elapsedTime.count()
    );
  }
}

TestRunSummary* TestRunner::RunGroup(const TestGroup* group, TestRunSummary* summary)
{
  if (!group) {
    return summary;
  }

#ifdef ENABLE_DEBUG
  TEST_LOG_PRINT(
      LogColor::Yellow, "[%s] - Test Count: %zu", group->GetName(), group->GetTests().size()
  );
#else
  if (strcmp(group->GetName(), "ROOT")) {
    TEST_LOG_PRINT(LogColor::Cyan, {}, "[{}]", group->GetName());
  }
#endif

  this->BeginContext(group->GetName(), "(before_all)");
  group->ExecuteBeforeAll();
  EndContext();

  auto groupStartTime = std::chrono::high_resolution_clock::now();

  const auto& tests = group->GetTests();
  for (size_t i = 0; i < tests.size(); ++i) {
    this->RunOne(group, &tests[i], summary);
  }

  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    this->RunGroup(group->GetChild(i), summary);
    std::printf("\n");
  }

  auto groupEndTime = std::chrono::high_resolution_clock::now();
  TestDuration groupElapsedTime = groupEndTime - groupStartTime;

  if (strcmp(group->GetName(), "ROOT") != 0) {
    TEST_LOG_PRINT(
        LogColor::Yellow, {}, "[{}] group total: {:.4f}ms", group->GetName(),
        groupElapsedTime.count()
    );
  }

  this->BeginContext(group->GetName(), "(after_all)");
  group->ExecuteAfterAll();
  EndContext();

  return summary;
}

TestRunSummary TestRunner::RunAll(const TestRegistry* registry)
{

  if (!registry || !registry->GetRootGroup()) {
    TEST_LOG_ERROR("root TestGroup was unable to be set on default registry");
    std::abort();
  }

  TestRunSummary summary;

  auto suiteStartTime = std::chrono::high_resolution_clock::now();

  this->RunGroup(registry->GetRootGroup(), &summary);

  auto suiteEndTime = std::chrono::high_resolution_clock::now();
  summary.totalElapsedTime = suiteEndTime - suiteStartTime;

  // Print summary
  std::printf("\n");
  TEST_LOG_PRINT(LogColor::Default, {}, "─────────────────────────────────");

  std::printf(
      LOG_COLOR_CODE_DEFAULT "\nSummary: " LOG_COLOR_CODE_YELLOW "%d total" LOG_COLOR_CODE_DEFAULT
                             ", " LOG_COLOR_CODE_GREEN "%d passed" LOG_COLOR_CODE_DEFAULT
                             ", " LOG_COLOR_CODE_RED "%d failed\n\n" LOG_COLOR_CODE_DEFAULT,
      summary.total, summary.passed, summary.failed
  );

  if (summary.total > 0) {
    TEST_LOG_PRINT(
        LogColor::Yellow, {}, "Slowest: [{}] {} ({:.4f}ms)", summary.slowestTestGroupName,
        summary.slowestTestName, summary.slowestTestElapsedTime.count()
    );
  }

  TEST_LOG_PRINT(LogColor::Default, {}, "─────────────────────────────────");

  return summary;
}
} // namespace Umbra::Test
