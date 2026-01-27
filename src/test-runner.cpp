#include "umbra/test-runner.hpp"
#include "umbra/test-fail-handler-registry.hpp"
#include "umbra/test-group.hpp"
#include "umbra/test-log.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>

namespace Umbra::Test {

static TestRunner* g_runner = nullptr;
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
  test->Run();
  EndContext();

  this->BeginContext(group->GetName(), "(after_each)");
  group->ExecuteAfterEach();
  EndContext();

  summary->total += 1;
  if (this->isFailure) {
    summary->failed += 1;
  }
  else {
    summary->passed += 1;
    TEST_LOG_PRINT(
        LogColor::Green, {}, "PASS [{}] {}", CheckGroupName(group->GetName()), test->GetName()
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

  const auto& tests = group->GetTests();
  for (size_t i = 0; i < tests.size(); ++i) {
    this->RunOne(group, &tests[i], summary);
  }

  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    this->RunGroup(group->GetChild(i), summary);
    std::printf("\n");
  }

  this->BeginContext(group->GetName(), "(after_all)");
  group->ExecuteAfterAll();
  EndContext();

  return summary;
}

TestRunSummary TestRunner::RunAll(const TestRegistry* registry)
{
  g_runner = this;

  if (!g_runner) {
    TEST_LOG_ERROR("Runner was unable to be set");
    std::abort();
  }

  if (!registry || !registry->GetRootGroup()) {
    TEST_LOG_ERROR("root TestGroup was unable to be set on default registry");
    std::abort();
  }

  TestRunSummary summary = {0, 0, 0};

  this->RunGroup(registry->GetRootGroup(), &summary);

  g_runner = nullptr;

  std::printf(
      LOG_COLOR_CODE_DEFAULT "\nSummary: " LOG_COLOR_CODE_YELLOW "%d total" LOG_COLOR_CODE_DEFAULT
                           ", " LOG_COLOR_CODE_GREEN "%d passed" LOG_COLOR_CODE_DEFAULT
                           ", " LOG_COLOR_CODE_RED "%d failed\n\n" LOG_COLOR_CODE_DEFAULT,
      summary.total, summary.passed, summary.failed
  );

  return summary;
}
} // namespace Umbra::Test
