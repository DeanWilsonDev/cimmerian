#include "cimmerian/test-runner.hpp"
#include "cimmerian/ansi-codes.hpp"
#include "cimmerian/ansi-text-builder.hpp"
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
#include "cimmerian/snapshot/hash-snapshot-store.hpp"
#include "cimmerian/snapshot/inline-snapshot-rewriter.hpp"
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include "cimmerian/snapshot/string-snapshot-store.hpp"
#endif
#include "cimmerian/test-fail-handler-registry.hpp"
#include "cimmerian/test-group.hpp"
#include "cimmerian/test-log.hpp"
#include "cimmerian/test-mode.hpp"
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>

namespace Cimmerian {

using namespace Cimmerian::Log;

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
  activeInstance = this;
}

static std::string ExtractBasename(const std::string& filePath)
{
  const std::size_t lastSlashPosition = filePath.find_last_of("/\\");
  return (lastSlashPosition == std::string::npos) ? filePath : filePath.substr(lastSlashPosition + 1);
}

static void PrintFailureBlock(const TestFailRecord& failure)
{
  constexpr const char* BLOCK_INDENT  = "      ";
  constexpr const char* DETAIL_INDENT = "            ";

  const std::string filename = ExtractBasename(failure.file);

  std::printf(
      "\n%s%s%s%s\n", BLOCK_INDENT, Ansi::ANSI_COLOR_BRIGHT_YELLOW, failure.message.c_str(),
      Ansi::ANSI_RESET
  );

  if (!failure.details.empty()) {
    std::printf("\n");
    for (const std::string& detailLine : failure.details) {
      if (detailLine.empty()) {
        std::printf("\n");
      }
      else {
        std::printf("%s%s\n", DETAIL_INDENT, detailLine.c_str());
      }
    }
  }

  if (failure.file.empty()) {
    return;
  }

  std::printf(
      "\n%s%sat %s:%d%s\n",
      BLOCK_INDENT, Ansi::ANSI_COLOR_BRIGHT_WHITE,
      filename.c_str(), failure.line, Ansi::ANSI_RESET
  );
}

void TestRunner::OnTestFail(const TestFailRecord& failure)
{
  if (!this->inTest) {
    std::fprintf(
        stderr, "%s" TAG_ERROR "%s:%d: test failure outside of running test: %s\n",
        Ansi::ANSI_COLOR_BRIGHT_RED, failure.file.c_str(), failure.line, failure.message.c_str()
    );
    for (const std::string& detailLine : failure.details) {
      std::fprintf(stderr, "    %s\n", detailLine.c_str());
    }
    return;
  }

  this->isFailure = true;
  this->totalFailures++;
  this->pendingFailures.push_back(failure);
}

template <typename TPredicate>
static bool AnyTestInSubtree(const TestGroup* group, TPredicate predicate)
{
  for (const TestCase& test : group->GetTests()) {
    if (predicate(test)) {
      return true;
    }
  }
  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    if (AnyTestInSubtree(group->GetChild(i), predicate)) {
      return true;
    }
  }
  return false;
}

static bool WillRun(const TestCase& test)
{
  return TestModeRegistry::GetInstance().ShouldRun(test.GetMode());
}

// Tests left out by an _ONLY elsewhere aren't printed, so a group made up
// entirely of those stays out of the output.
static bool WillPrint(const TestCase& test)
{
  return test.GetMode() == TestMode::Skip || WillRun(test);
}

static void CollectForbiddenTests(const TestGroup* group, std::vector<std::string>* forbiddenTests)
{
  const TestModeRegistry& modes = TestModeRegistry::GetInstance();
  for (const TestCase& test : group->GetTests()) {
    if (modes.IsForbidden(test.GetMode())) {
      const std::string groupPath = BuildGroupPath(group);
      forbiddenTests->push_back(
          (groupPath.empty() ? "" : groupPath + " > ") + test.GetName() + "  (" +
          ForbiddenModeReason(test.GetMode()) + ")"
      );
    }
  }
  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    CollectForbiddenTests(group->GetChild(i), forbiddenTests);
  }
}

void TestRunner::RunOne(const TestGroup* group, const TestCase* test, TestRunSummary* summary)
{
  summary->total++;

  if (test->GetMode() == TestMode::Skip) {
    summary->skipped++;
    TEST_LOG_PRINT(
        LogColor::Yellow, "[SKIP] [{}] {}", CheckGroupName(group->GetName()), test->GetName()
    );
    return;
  }

  if (!WillRun(*test)) {
    summary->skipped++;
    return;
  }

  this->isFailure = false;
  this->currentGroupPath = BuildGroupPath(group);

  this->BeginContext(group->GetName(), "(before_each)");
  const bool isSetUp = this->RunContained("BEFORE_EACH", [&] { group->ExecuteBeforeEach(); });
  EndContext();

  // A BEFORE_EACH that threw left the test's fixture half set up, so its body
  // doesn't run; AFTER_EACH still does, to tear down what was set up.
  this->BeginContext(group->GetName(), test->GetName());
  auto startTime = std::chrono::high_resolution_clock::now();
  if (isSetUp) {
    this->RunContained("The test", [&] { test->Run(); });
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  EndContext();

  this->BeginContext(group->GetName(), "(after_each)");
  this->RunContained("AFTER_EACH", [&] { group->ExecuteAfterEach(); });
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

  if (this->isFailure) {
    summary->failed++;
    TEST_LOG_PRINT(
        LogColor::Red, "[FAIL] [{}] {}  ({:.4f}ms)", CheckGroupName(group->GetName()),
        test->GetName(), elapsedTime.count()
    );
    for (const TestFailRecord& failureRecord : this->pendingFailures) {
      PrintFailureBlock(failureRecord);
    }
    this->pendingFailures.clear();
    std::printf("\n");
  }
  else {
    summary->passed++;
    TEST_LOG_PRINT(
        LogColor::Green, "[PASS] [{}] {}  ({:.4f}ms)", CheckGroupName(group->GetName()),
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
  if (strcmp(group->GetName(), "ROOT") && AnyTestInSubtree(group, WillPrint)) {
    TEST_LOG_PRINT(LogColor::Cyan, "[{}]", group->GetName());
  }
#endif

  // Skipped tests don't need the group's fixtures set up.
  const bool runsAnyTest = AnyTestInSubtree(group, WillRun);

  if (runsAnyTest) {
    this->BeginContext(group->GetName(), "(before_all)");
    group->ExecuteBeforeAll();
    EndContext();
  }

  auto groupStartTime = std::chrono::high_resolution_clock::now();

  const auto& tests = group->GetTests();
  for (size_t i = 0; i < tests.size(); ++i) {
    this->RunOne(group, &tests[i], summary);
  }

  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    this->RunGroup(group->GetChild(i), summary);
    if (AnyTestInSubtree(group->GetChild(i), WillPrint)) {
      std::printf("\n");
    }
  }

  auto groupEndTime = std::chrono::high_resolution_clock::now();
  TestDuration groupElapsedTime = groupEndTime - groupStartTime;

  if (strcmp(group->GetName(), "ROOT") != 0 && runsAnyTest) {
    TEST_LOG_PRINT(
        LogColor::Yellow, "[{}] group total: {:.4f}ms", group->GetName(), groupElapsedTime.count()
    );
  }

  if (runsAnyTest) {
    this->BeginContext(group->GetName(), "(after_all)");
    group->ExecuteAfterAll();
    EndContext();
  }

  return summary;
}

TestRunSummary TestRunner::RunAll(const TestRegistry* registry)
{

  if (!registry || !registry->GetRootGroup()) {
    TEST_LOG_ERROR("root TestGroup was unable to be set on default registry");
    std::abort();
  }

  TestRunSummary summary;

  std::vector<std::string> forbiddenTests;
  CollectForbiddenTests(registry->GetRootGroup(), &forbiddenTests);
  if (!forbiddenTests.empty()) {
    TEST_LOG_ERROR("{} forbidden test(s) found, not running any tests:", forbiddenTests.size());
    for (const std::string& forbiddenTest : forbiddenTests) {
      TEST_LOG_PRINT(LogColor::Red, "  {}", forbiddenTest);
    }
    // Counted as failures so entry points that exit on summary.failed fail the run.
    summary.failed = static_cast<int>(forbiddenTests.size());
    return summary;
  }

  auto suiteStartTime = std::chrono::high_resolution_clock::now();

  this->RunGroup(registry->GetRootGroup(), &summary);

  auto suiteEndTime = std::chrono::high_resolution_clock::now();
  summary.totalElapsedTime = suiteEndTime - suiteStartTime;

#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
  Snapshot::InlineSnapshotRewriter::GetInstance().FlushAll();
  Snapshot::StringSnapshotStore::GetInstance().Flush();
  Snapshot::HashSnapshotStore::GetInstance().Flush();

  const Snapshot::SnapshotSummary& snapshotSummary = Snapshot::SnapshotSummaryAccumulator::GetInstance().Get();
  summary.snapshotsMatched = snapshotSummary.snapshotsMatched;
  summary.snapshotsFailed = snapshotSummary.snapshotsFailed;
  summary.snapshotsUpdated = snapshotSummary.snapshotsUpdated;
  summary.snapshotsMissing = snapshotSummary.snapshotsMissing;
  summary.inlineRewriteCount = snapshotSummary.inlineRewriteCount;
#endif

  // Print summary
  std::printf("\n");
  std::printf("────────────────────────────────────────────────");

  std::printf(
      "\nSummary: %s%d total%s, %s%d passed%s, %s%d failed%s", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
      summary.total, Ansi::ANSI_RESET, Ansi::ANSI_COLOR_BRIGHT_GREEN, summary.passed,
      Ansi::ANSI_RESET, Ansi::ANSI_COLOR_BRIGHT_RED, summary.failed, Ansi::ANSI_RESET
  );
  if (summary.skipped > 0) {
    std::printf(
        ", %s%d skipped%s", Ansi::ANSI_COLOR_BRIGHT_YELLOW, summary.skipped, Ansi::ANSI_RESET
    );
  }
  std::printf("\n\n");

  if (TestModeRegistry::GetInstance().HasFocusedTests()) {
    std::printf(
        "%sFocused run: only *_ONLY tests were run%s\n",
        Ansi::ANSI_COLOR_BRIGHT_YELLOW, Ansi::ANSI_RESET
    );
  }

  if (summary.passed + summary.failed > 0) {
    std::printf(
        "%sSlowest: [%s] %s (%.4fms)\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
        summary.slowestTestGroupName.c_str(), summary.slowestTestName.c_str(),
        summary.slowestTestElapsedTime.count()
    );
  }

  std::printf("────────────────────────────────────────────────\n");

  const int totalSnapshotActivity =
      summary.snapshotsMatched + summary.snapshotsFailed + summary.snapshotsUpdated + summary.snapshotsMissing;
  if (totalSnapshotActivity > 0) {
    std::printf(
        "Snapshots: %d matched, %d failed, %d updated, %d missing", summary.snapshotsMatched,
        summary.snapshotsFailed, summary.snapshotsUpdated, summary.snapshotsMissing
    );
    if (summary.inlineRewriteCount > 0) {
      std::printf(" (%d source file(s) rewritten)", summary.inlineRewriteCount);
    }
    std::printf("\n────────────────────────────────────────────────\n");
  }

  return summary;
}
} // namespace Cimmerian
