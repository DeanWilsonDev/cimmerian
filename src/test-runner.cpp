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
#include "cimmerian/test-selection.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <format>
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
  TestFailHandlerRegistry& handlers = TestFailHandlerRegistry::GetInstance();
  this->previousHandler = handlers.GetHandler();
  this->previousActive = activeInstance;
  handlers.RegisterHandler(this);
  activeInstance = this;
}

TestRunner::~TestRunner()
{
  TestFailHandlerRegistry& handlers = TestFailHandlerRegistry::GetInstance();
  if (handlers.GetHandler() == this) {
    handlers.RegisterHandler(this->previousHandler);
  }
  if (activeInstance == this) {
    activeInstance = this->previousActive;
  }
}

static std::string ExtractBasename(const std::string& filePath)
{
  const std::size_t lastSlashPosition = filePath.find_last_of("/\\");
  return (lastSlashPosition == std::string::npos) ? filePath : filePath.substr(lastSlashPosition + 1);
}

static void PrintFailureBlock(std::FILE* output, const TestFailRecord& failure)
{
  constexpr const char* BLOCK_INDENT  = "      ";
  constexpr const char* DETAIL_INDENT = "            ";

  const std::string filename = ExtractBasename(failure.file);

  std::fprintf(
      output, "\n%s%s%s%s\n", BLOCK_INDENT, Ansi::ANSI_COLOR_BRIGHT_YELLOW, failure.message.c_str(),
      Ansi::ANSI_RESET
  );

  if (!failure.details.empty()) {
    std::fprintf(output, "\n");
    for (const std::string& detailLine : failure.details) {
      if (detailLine.empty()) {
        std::fprintf(output, "\n");
      }
      else {
        std::fprintf(output, "%s%s\n", DETAIL_INDENT, detailLine.c_str());
      }
    }
  }

  if (failure.file.empty()) {
    return;
  }

  std::fprintf(
      output, "\n%s%sat %s:%d%s\n", BLOCK_INDENT, Ansi::ANSI_COLOR_BRIGHT_WHITE, filename.c_str(),
      failure.line, Ansi::ANSI_RESET
  );
}

void TestRunner::PrintFailures()
{
  for (const TestFailRecord& failureRecord : this->pendingFailures) {
    PrintFailureBlock(this->output, failureRecord);
  }
  this->pendingFailures.clear();
  std::fprintf(this->output, "\n");
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

template <typename TPredicate>
static int CountTestsInSubtree(const TestGroup* group, TPredicate predicate)
{
  int count = 0;
  for (const TestCase& test : group->GetTests()) {
    if (predicate(test)) {
      count++;
    }
  }
  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    count += CountTestsInSubtree(group->GetChild(i), predicate);
  }
  return count;
}

bool TestRunner::IsInScope(const TestCase& test) const
{
  return !this->hasScope || this->testsInScope.contains(&test);
}

bool TestRunner::WillRun(const TestCase& test) const
{
  return this->IsInScope(test) && TestModeRegistry::GetInstance().ShouldRun(test.GetMode());
}

// Tests left out by an _ONLY elsewhere aren't printed, so a group made up
// entirely of those stays out of the output.
bool TestRunner::WillPrint(const TestCase& test) const
{
  return this->IsInScope(test) && (test.GetMode() == TestMode::Skip || this->WillRun(test));
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
  if (!this->IsInScope(*test)) {
    return;
  }

  summary->total++;

  if (test->GetMode() == TestMode::Skip) {
    summary->skipped++;
    TestLogPrintTo(
        this->output, LogColor::Yellow, "[SKIP] [{}] {}", CheckGroupName(group->GetName()),
        test->GetName()
    );
    return;
  }

  if (!this->WillRun(*test)) {
    summary->skipped++;
    return;
  }

  // The fixture this test needs was never set up.
  if (this->failedSetupGroup != nullptr) {
    summary->failed++;
    const std::string reason =
        (this->failedSetupGroup == group)
            ? std::string("BEFORE_ALL failed")
            : "BEFORE_ALL in [" + std::string(this->failedSetupGroup->GetName()) + "] failed";
    TestLogPrintTo(
        this->output, LogColor::Red, "[FAIL] [{}] {}  (not run: {})",
        CheckGroupName(group->GetName()), test->GetName(), reason
    );
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
    TestLogPrintTo(
        this->output, LogColor::Red, "[FAIL] [{}] {}  ({:.4f}ms)", CheckGroupName(group->GetName()),
        test->GetName(), elapsedTime.count()
    );
    this->PrintFailures();
  }
  else {
    summary->passed++;
    TestLogPrintTo(
        this->output, LogColor::Green, "[PASS] [{}] {}  ({:.4f}ms)",
        CheckGroupName(group->GetName()), test->GetName(), elapsedTime.count()
    );
  }
}

bool TestRunner::RunGroupHook(
    const TestGroup* group,
    const char* hookName,
    void (TestGroup::*hook)() const,
    TestRunSummary* summary
)
{
  // What GetCurrentTest() has always returned inside these hooks.
  const char* contextName =
      (std::strcmp(hookName, "BEFORE_ALL") == 0) ? "(before_all)" : "(after_all)";

  this->isFailure = false;
  this->currentGroupPath = BuildGroupPath(group);
  this->BeginContext(group->GetName(), contextName);
  this->RunContained(hookName, [&] { (group->*hook)(); });
  this->EndContext();

  if (!this->isFailure) {
    return true;
  }

  TestLogPrintTo(
      this->output, LogColor::Red, "[FAIL] [{}] {}", CheckGroupName(group->GetName()), hookName
  );
  this->PrintFailures();
  summary->hookFailures.push_back({.groupPath = this->currentGroupPath, .hookName = hookName});
  return false;
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
#endif

  const auto willRun = [this](const TestCase& test) { return this->WillRun(test); };
  const auto willPrint = [this](const TestCase& test) { return this->WillPrint(test); };

#ifndef ENABLE_DEBUG
  if (strcmp(group->GetName(), "ROOT") && AnyTestInSubtree(group, willPrint)) {
    TestLogPrintTo(this->output, LogColor::Cyan, "[{}]", group->GetName());
  }
#endif

  // Skipped tests don't need the group's fixtures set up, and a group under a
  // failed BEFORE_ALL sets nothing up.
  const bool runsAnyTest = AnyTestInSubtree(group, willRun);
  const bool runsHooks = runsAnyTest && this->failedSetupGroup == nullptr;

  // As in Jest, a failed BEFORE_ALL fails every test under its group, nested
  // groups included, without running them. AFTER_ALL still runs, to tear down
  // whatever BEFORE_ALL did set up.
  if (runsHooks &&
      !this->RunGroupHook(group, "BEFORE_ALL", &TestGroup::ExecuteBeforeAll, summary)) {
    this->failedSetupGroup = group;
    summary->hookFailures.back().testsNotRun = CountTestsInSubtree(group, willRun);
  }
  const bool runsTests = this->failedSetupGroup == nullptr;

  // At the root, each top-level group is timed whole for --save-timings, as
  // are the tests outside every DESCRIBE.
  const bool isRoot = group->GetParent() == nullptr;
  const auto recordTiming = [&](const char* groupName, int testCount, auto startTime) {
    if (isRoot && testCount > 0) {
      const auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
      summary->groupTimings.push_back(
          {.groupName = groupName,
           .testCount = testCount,
           .microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()}
      );
    }
  };

  auto groupStartTime = std::chrono::high_resolution_clock::now();

  const auto& tests = group->GetTests();
  for (size_t i = 0; i < tests.size(); ++i) {
    this->RunOne(group, &tests[i], summary);
  }
  recordTiming(
      "", static_cast<int>(std::count_if(tests.begin(), tests.end(), willRun)), groupStartTime
  );

  for (size_t i = 0; i < group->GetChildCount(); ++i) {
    const TestGroup* child = group->GetChild(i);
    const auto childStartTime = std::chrono::high_resolution_clock::now();
    this->RunGroup(child, summary);
    recordTiming(child->GetName(), CountTestsInSubtree(child, willRun), childStartTime);
    if (AnyTestInSubtree(child, willPrint)) {
      std::fprintf(this->output, "\n");
    }
  }

  auto groupEndTime = std::chrono::high_resolution_clock::now();
  TestDuration groupElapsedTime = groupEndTime - groupStartTime;

  if (strcmp(group->GetName(), "ROOT") != 0 && runsAnyTest && runsTests) {
    TestLogPrintTo(
        this->output, LogColor::Yellow, "[{}] group total: {:.4f}ms", group->GetName(),
        groupElapsedTime.count()
    );
  }

  if (this->failedSetupGroup == group) {
    this->failedSetupGroup = nullptr;
  }

  // A failed AFTER_ALL fails no test, so it counts as a failure itself.
  if (runsHooks && !this->RunGroupHook(group, "AFTER_ALL", &TestGroup::ExecuteAfterAll, summary)) {
    summary->failed++;
  }

  return summary;
}

TestRunSummary TestRunner::RunAll(const TestRegistry* registry)
{

  if (!registry || !registry->GetRootGroup()) {
    TestLogPrintTo(
        this->output, LogColor::Red,
        TAG_ERROR "root TestGroup was unable to be set on default registry"
    );
    // abort() doesn't flush, and the message is the only clue to why the run died.
    std::fflush(this->output);
    std::abort();
  }

  TestRunSummary summary;

  // Command-line errors are counted as a failure, like forbidden tests below,
  // so entry points that exit on summary.failed fail the run.
  const TestSelectionOptions& selectionOptions = TestSelectionRegistry::GetInstance().Get();
  if (!selectionOptions.error.empty()) {
    TestLogPrintTo(
        this->output, LogColor::Red, TAG_ERROR "{}; not running any tests", selectionOptions.error
    );
    summary.failed = 1;
    return summary;
  }

#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
  if (selectionOptions.shard && Snapshot::SnapshotRunModeRegistry::GetInstance().IsUpdateMode()) {
    TestLogPrintTo(
        this->output, LogColor::Red,
        TAG_ERROR
        "--update-snapshots can't be used with --shard, since parallel shards would write "
        "the same snapshot files; update them in one unsharded run. Not running any tests"
    );
    summary.failed = 1;
    return summary;
  }
#endif

  std::vector<std::string> forbiddenTests;
  CollectForbiddenTests(registry->GetRootGroup(), &forbiddenTests);
  if (!forbiddenTests.empty()) {
    TestLogPrintTo(
        this->output, LogColor::Red,
        TAG_ERROR "{} forbidden test(s) found, not running any tests:", forbiddenTests.size()
    );
    for (const std::string& forbiddenTest : forbiddenTests) {
      TestLogPrintTo(this->output, LogColor::Red, "  {}", forbiddenTest);
    }
    // Counted as failures so entry points that exit on summary.failed fail the run.
    summary.failed = static_cast<int>(forbiddenTests.size());
    return summary;
  }

  // Every shard has to read the same timings, or they split the tests
  // differently and a test runs twice or not at all. So a file that can't be
  // read stops the run, rather than quietly falling back to test counts.
  std::vector<GroupTiming> timings;
  if (selectionOptions.shard && !selectionOptions.timingsPath.empty()) {
    LoadedTimings loaded = LoadTimings(selectionOptions.timingsPath);
    if (!loaded.error.empty()) {
      TestLogPrintTo(
          this->output, LogColor::Red,
          TAG_ERROR "--timings: {}. Not running any tests, since every shard must balance by the "
                    "same timings",
          loaded.error
      );
      summary.failed = 1;
      return summary;
    }
    timings = std::move(loaded.timings);
    this->shardBalance =
        loaded.found
            ? std::format("balanced by timings in {}", selectionOptions.timingsPath)
            : std::format(
                  "balanced by test count, since {} doesn't exist", selectionOptions.timingsPath
              );
  }

  const TestSelection selection = SelectTests(
      registry->GetRootGroup(), selectionOptions.filter, selectionOptions.shard,
      [](const TestCase& test) {
        return TestModeRegistry::GetInstance().ShouldRun(test.GetMode());
      },
      timings
  );
  this->testsInScope.clear();
  for (const SelectedTest& selected : selection.tests) {
    this->testsInScope.insert(selected.test);
  }
  this->hasScope = true;

  // Only the names, one per line and without colour, so a script can read them.
  if (selectionOptions.listOnly) {
    for (const SelectedTest& selected : selection.tests) {
      if (this->WillRun(*selected.test)) {
        std::fprintf(this->output, "%s\n", selected.fullName.c_str());
      }
    }
    return summary;
  }

  auto suiteStartTime = std::chrono::high_resolution_clock::now();

  this->RunGroup(registry->GetRootGroup(), &summary);

  auto suiteEndTime = std::chrono::high_resolution_clock::now();
  summary.totalElapsedTime = suiteEndTime - suiteStartTime;

  if (!selectionOptions.saveTimingsPath.empty()) {
    std::string saveError;
    this->savedTimingsFailed =
        !SaveTimings(selectionOptions.saveTimingsPath, summary.groupTimings, &saveError);
    this->savedTimings = this->savedTimingsFailed
                             ? std::format("--save-timings: {}", saveError)
                             : std::format(
                                   "Saved the times of {} group(s) to {}",
                                   summary.groupTimings.size(), selectionOptions.saveTimingsPath
                               );
  }

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

  this->PrintSummary(summary, selection);
  return summary;
}

void TestRunner::PrintSummary(const TestRunSummary& summary, const TestSelection& selection)
{
  const TestSelectionOptions& selectionOptions = TestSelectionRegistry::GetInstance().Get();
  std::FILE* output = this->output;

  // A failed AFTER_ALL is counted in summary.failed but isn't a failed test.
  int failedAfterAllHooks = 0;
  for (const GroupHookFailure& hookFailure : summary.hookFailures) {
    if (hookFailure.hookName == "AFTER_ALL") {
      failedAfterAllHooks++;
    }
  }

  std::fprintf(output, "\n");
  std::fprintf(output, "────────────────────────────────────────────────");

  std::fprintf(
      output, "\nSummary: %s%d total%s, %s%d passed%s, %s%d failed%s",
      Ansi::ANSI_COLOR_BRIGHT_YELLOW, summary.total, Ansi::ANSI_RESET,
      Ansi::ANSI_COLOR_BRIGHT_GREEN, summary.passed, Ansi::ANSI_RESET, Ansi::ANSI_COLOR_BRIGHT_RED,
      summary.failed - failedAfterAllHooks, Ansi::ANSI_RESET
  );
  if (summary.skipped > 0) {
    std::fprintf(
        output, ", %s%d skipped%s", Ansi::ANSI_COLOR_BRIGHT_YELLOW, summary.skipped,
        Ansi::ANSI_RESET
    );
  }
  std::fprintf(output, "\n");

  // A failed hook fails the run even when every test passed, so say which.
  for (const GroupHookFailure& hookFailure : summary.hookFailures) {
    std::fprintf(
        output, "%s%s failed in [%s]", Ansi::ANSI_COLOR_BRIGHT_RED, hookFailure.hookName.c_str(),
        hookFailure.groupPath.c_str()
    );
    if (hookFailure.testsNotRun > 0) {
      std::fprintf(
          output, ", so %d %s didn't run", hookFailure.testsNotRun,
          hookFailure.testsNotRun == 1 ? "test" : "tests"
      );
    }
    std::fprintf(output, "%s\n", Ansi::ANSI_RESET);
  }
  std::fprintf(output, "\n");

  if (TestModeRegistry::GetInstance().HasFocusedTests()) {
    std::fprintf(
        output, "%sFocused run: only *_ONLY tests were run%s\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
        Ansi::ANSI_RESET
    );
  }

  if (!selectionOptions.filter.empty()) {
    if (selection.matchedCount == 0) {
      std::fprintf(
          output, "%s--filter \"%s\" matched no tests%s\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
          selectionOptions.filter.c_str(), Ansi::ANSI_RESET
      );
    }
    else {
      std::fprintf(
          output, "%sFilter \"%s\": %d of %d tests%s\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
          selectionOptions.filter.c_str(), selection.matchedCount, selection.registeredCount,
          Ansi::ANSI_RESET
      );
    }
  }

  if (selectionOptions.shard) {
    std::fprintf(
        output, "%sShard %d/%d: %zu of %d tests%s%s%s\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
        selectionOptions.shard->index, selectionOptions.shard->count, selection.tests.size(),
        selection.matchedCount, this->shardBalance.empty() ? "" : ", ", this->shardBalance.c_str(),
        Ansi::ANSI_RESET
    );
  }

  if (!this->savedTimings.empty()) {
    std::fprintf(
        output, "%s%s%s%s\n",
        this->savedTimingsFailed ? Ansi::ANSI_COLOR_BRIGHT_RED : Ansi::ANSI_COLOR_BRIGHT_YELLOW,
        this->savedTimingsFailed ? TAG_ERROR : "", this->savedTimings.c_str(), Ansi::ANSI_RESET
    );
  }

  if (!summary.perTestTimings.empty()) {
    std::fprintf(
        output, "%sSlowest: [%s] %s (%.4fms)\n", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
        summary.slowestTestGroupName.c_str(), summary.slowestTestName.c_str(),
        summary.slowestTestElapsedTime.count()
    );
  }

  std::fprintf(output, "────────────────────────────────────────────────\n");

  const int totalSnapshotActivity =
      summary.snapshotsMatched + summary.snapshotsFailed + summary.snapshotsUpdated + summary.snapshotsMissing;
  if (totalSnapshotActivity > 0) {
    std::fprintf(
        output, "Snapshots: %d matched, %d failed, %d updated, %d missing",
        summary.snapshotsMatched, summary.snapshotsFailed, summary.snapshotsUpdated,
        summary.snapshotsMissing
    );
    if (summary.inlineRewriteCount > 0) {
      std::fprintf(output, " (%d source file(s) rewritten)", summary.inlineRewriteCount);
    }
    std::fprintf(output, "\n────────────────────────────────────────────────\n");
  }
}
} // namespace Cimmerian
