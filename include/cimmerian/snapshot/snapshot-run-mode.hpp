#pragma once

#include <optional>
#include <string>

namespace Cimmerian::Snapshot {

enum class RunMode {
  Verify,  // default: compare against stored snapshots, fail on mismatch
  Update,  // accept current output as the new golden
};

// Process-wide snapshot run mode / configuration, shared by string, inline
// and hash snapshots (and readable by the visual regression extension so a
// single --update-snapshots flag can drive both systems).
class SnapshotRunModeRegistry {
public:
  static SnapshotRunModeRegistry& GetInstance();

  SnapshotRunModeRegistry(const SnapshotRunModeRegistry&) = delete;
  SnapshotRunModeRegistry& operator=(const SnapshotRunModeRegistry&) = delete;

  void SetMode(RunMode mode) { this->mode = mode; }
  RunMode GetMode() const { return this->mode; }
  bool IsUpdateMode() const { return this->mode == RunMode::Update; }

  void SetSnapshotRootOverride(const std::string& dir)
  {
    this->snapshotRootOverride = dir;
  }
  const std::optional<std::string>& GetSnapshotRootOverride() const
  {
    return this->snapshotRootOverride;
  }

  // Parses --update-snapshots and --snapshot-dir <path> out of argv, applying
  // them to this registry. Other arguments are ignored.
  void ParseArgs(int argc, char* argv[]);

private:
  SnapshotRunModeRegistry() = default;

  RunMode mode = RunMode::Verify;
  std::optional<std::string> snapshotRootOverride;
};

struct SnapshotSummary {
  int snapshotsMatched = 0;
  int snapshotsFailed = 0;
  int snapshotsUpdated = 0;   // in Update mode
  int snapshotsMissing = 0;   // golden didn't exist, written on first run
  int inlineRewriteCount = 0; // source files rewritten
};

// Accumulates snapshot outcomes across a whole run so the entry point can
// merge them into TestRunSummary without every snapshot store needing to
// know about the core test runner.
class SnapshotSummaryAccumulator {
public:
  static SnapshotSummaryAccumulator& GetInstance();

  SnapshotSummaryAccumulator(const SnapshotSummaryAccumulator&) = delete;
  SnapshotSummaryAccumulator& operator=(const SnapshotSummaryAccumulator&) = delete;

  void RecordMatched() { ++this->summary.snapshotsMatched; }
  void RecordFailed() { ++this->summary.snapshotsFailed; }
  void RecordUpdated() { ++this->summary.snapshotsUpdated; }
  void RecordMissing() { ++this->summary.snapshotsMissing; }
  void RecordInlineRewrite() { ++this->summary.inlineRewriteCount; }

  const SnapshotSummary& Get() const { return this->summary; }
  void Reset() { this->summary = SnapshotSummary{}; }

private:
  SnapshotSummaryAccumulator() = default;
  SnapshotSummary summary;
};

} // namespace Cimmerian::Snapshot
