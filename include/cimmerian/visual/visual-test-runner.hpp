#pragma once

#include <memory>
#include <string>
#include <vector>
#include "i-event-injector.hpp"
#include "i-screen-capture.hpp"
#include "image-diff.hpp"
#include "snapshot-store.hpp"
#include "visual-test-registry.hpp"

namespace Cimmerian::Visual {

enum class VisualRunMode {
  Verify, // default: compare against goldens, fail on mismatch
  Update, // write new goldens, always "pass"
  Review, // verify + emit HTML report; does not fail the process
};

struct VisualTestRunSummary {
  int total = 0;
  int passed = 0;
  int failed = 0;
  int updatedGoldens = 0; // in Update mode
  int missingGoldens = 0; // goldens that don't exist yet
  std::string reportPath; // in Review mode
};

// One row of report data for a single ASSERT_SNAPSHOT call, kept around so
// Review mode can render golden | actual | diff after the whole suite runs.
struct VisualSnapshotReportEntry {
  std::string groupName;
  std::string testName;
  std::string label;
  bool passed;
  bool missingGolden;
  float differencePercent;
  std::string goldenPath;
  std::string actualPath;
  std::string diffPath;
};

class VisualTestRunner {
public:
  VisualTestRunner();
  ~VisualTestRunner() = default;

  void SetMode(VisualRunMode mode) { this->mode = mode; }
  VisualRunMode GetMode() const { return this->mode; }

  void SetSnapshotRoot(const std::string& root);
  void SetCapture(std::shared_ptr<IScreenCapture> capture);
  void SetInjector(std::shared_ptr<IEventInjector> injector);
  void SetFilter(const std::string& pattern) { this->filter = pattern; }

  // Parses --update-snapshots, --review-snapshots, --snapshot-dir <path> and
  // --filter <pattern> out of argv.
  void ParseArgs(int argc, char* argv[]);

  VisualTestRunSummary RunAll(const VisualTestRegistry* registry);
  void RunGroup(const VisualTestGroup* group, VisualTestRunSummary* summary);
  void RunOne(const VisualTestGroup* group, const VisualTestCase* test, VisualTestRunSummary* summary);

  // Called by SEND / WAIT / ASSERT_SNAPSHOT macros while a visual test runs.
  void SendEvent(const UIEvent& event);
  void AssertSnapshot(const std::string& label, const DiffOptions& options = {});

  static VisualTestRunner* GetActive() { return activeInstance; }

  const std::string& GetCurrentGroupPath() const { return this->currentGroupPath; }
  const char* GetCurrentTestName() const { return this->currentTestName; }
  void* GetCurrentWindowHandle() const { return this->currentWindowHandle; }

private:
  VisualRunMode mode = VisualRunMode::Verify;
  std::string snapshotRoot = "./snapshots";
  std::shared_ptr<IScreenCapture> capture;
  std::shared_ptr<IEventInjector> injector;
  std::string filter;
  std::unique_ptr<SnapshotStore> store;

  std::string currentGroupPath;
  const char* currentTestName = nullptr;
  void* currentWindowHandle = nullptr;
  bool currentTestFailed = false;

  std::vector<VisualSnapshotReportEntry> reportEntries;

  static inline VisualTestRunner* activeInstance = nullptr;

  bool MatchesFilter(const std::string& fullyQualifiedName) const;
  void WriteReport(VisualTestRunSummary* summary) const;
};

} // namespace Cimmerian::Visual
