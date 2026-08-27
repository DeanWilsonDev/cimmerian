#pragma once

#include "cimmerian/test-runner.hpp"
#include "cimmerian/test-debug.hpp"
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#endif

int main(int argc, char* argv[])
{
  Cimmerian::CheckDebug(argc, argv);
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
  Cimmerian::Snapshot::SnapshotRunModeRegistry::GetInstance().ParseArgs(argc, argv);
#endif
  Cimmerian::TestRunner runner = Cimmerian::TestRunner();
  Cimmerian::TestRunSummary summary = runner.RunAll(&Cimmerian::TestRegistry().GetInstance());
  return (summary.failed == 0) ? 0 : 1;
}
