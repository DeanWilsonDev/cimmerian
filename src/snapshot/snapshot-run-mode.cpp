#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include <string_view>

namespace Cimmerian::Snapshot {

SnapshotRunModeRegistry& SnapshotRunModeRegistry::GetInstance()
{
  static SnapshotRunModeRegistry instance;
  return instance;
}

void SnapshotRunModeRegistry::ParseArgs(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--update-snapshots") {
      this->SetMode(RunMode::Update);
    }
    else if (arg == "--snapshot-dir" && i + 1 < argc) {
      this->SetSnapshotRootOverride(argv[++i]);
    }
  }
}

SnapshotSummaryAccumulator& SnapshotSummaryAccumulator::GetInstance()
{
  static SnapshotSummaryAccumulator instance;
  return instance;
}

} // namespace Cimmerian::Snapshot
