#pragma once

#include "cimmerian/test-debug.hpp"
#include "cimmerian/test-mode.hpp"
#include "cimmerian/test-selection.hpp"
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#endif

namespace Cimmerian {

// Reads every flag the unit runner uses: --debug, --forbid-only,
// --forbid-skip, --filter, --list and --shard, plus --update-snapshots and
// --snapshot-dir with snapshot testing. Call it once at the top of main. A
// visual runner reads its own flags with VisualTestRunner::ParseArgs.
inline void ParseArgs(int argc, char* argv[])
{
  CheckDebug(argc, argv);
  TestModeRegistry::GetInstance().ParseArgs(argc, argv);
  TestSelectionRegistry::GetInstance().ParseArgs(argc, argv);
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
  Snapshot::SnapshotRunModeRegistry::GetInstance().ParseArgs(argc, argv);
#endif
}

} // namespace Cimmerian
