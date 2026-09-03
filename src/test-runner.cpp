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
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <sstream>

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
  constexpr const char* BLOCK_INDENT   = "      ";
  constexpr const char* CONTENT_INDENT = "        ";

  const std::string filename = ExtractBasename(failure.file);

  std::printf("\n");

  // Split message into lines; first line is the reason header, rest is the diff.
  std::istringstream messageStream(failure.message);
  std::string messageLine;
  bool isReasonLine = true;

  while (std::getline(messageStream, messageLine)) {
    if (messageLine.empty()) continue;

    if (isReasonLine) {
      std::printf(
          "%s%s%s%s\n\n", BLOCK_INDENT,
          Ansi::ANSI_COLOR_BRIGHT_YELLOW, messageLine.c_str(), Ansi::ANSI_RESET
      );
      isReasonLine = false;
    }
    else {
      std::printf("%s%s\n", CONTENT_INDENT, messageLine.c_str());
    }
  }

  std::printf(
      "\n%s%sat %s:%d%s\n",
      BLOCK_INDENT, Ansi::ANSI_COLOR_BRIGHT_WHITE,
      filename.c_str(), failure.line, Ansi::ANSI_RESET
  );
}

void TestRunner::OnTestFail(const char* file, int line, const char* msg)
{
  if (!this->inTest) {
    std::fprintf(
        stderr, "%s" TAG_ERROR "%s:%d: test failure outside of running test: %s\n",
        Ansi::ANSI_COLOR_BRIGHT_RED, file, line, msg
    );
    return;
  }

  this->isFailure = true;
  this->totalFailures++;
  this->pendingFailures.push_back({file, line, msg});
}

void TestRunner::RunOne(const TestGroup* group, const TestCase* test, TestRunSummary* summary)
{
  this->isFailure = false;
  this->currentGroupPath = BuildGroupPath(group);

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
  if (strcmp(group->GetName(), "ROOT")) {
    TEST_LOG_PRINT(LogColor::Cyan, "[{}]", group->GetName());
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
        LogColor::Yellow, "[{}] group total: {:.4f}ms", group->GetName(), groupElapsedTime.count()
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
      "\nSummary: %s%d total%s, %s%d passed%s, %s%d failed\n\n%s", Ansi::ANSI_COLOR_BRIGHT_YELLOW,
      summary.total, Ansi::ANSI_RESET, Ansi::ANSI_COLOR_BRIGHT_GREEN, summary.passed,
      Ansi::ANSI_RESET, Ansi::ANSI_COLOR_BRIGHT_RED, summary.failed, Ansi::ANSI_RESET
  );

  if (summary.total > 0) {
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
