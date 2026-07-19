#include <cimmerian/snapshot/snapshot-run-mode.hpp>
#include <cimmerian/test-debug.hpp>
#include <cimmerian/test-runner.hpp>
#include <cimmerian/visual.hpp>
#include <memory>

int main(int argc, char* argv[])
{
  Cimmerian::CheckDebug(argc, argv);
  Cimmerian::Snapshot::SnapshotRunModeRegistry::GetInstance().ParseArgs(argc, argv);

  Cimmerian::TestRunner runner;
  Cimmerian::TestRunSummary summary = runner.RunAll(&Cimmerian::TestRegistry::GetInstance());

  Cimmerian::Visual::VisualTestRunner visualRunner;
  visualRunner.ParseArgs(argc, argv);
#if defined(__unix__) || defined(__linux__)
  visualRunner.SetCapture(std::make_shared<Cimmerian::Visual::X11ScreenCapture>());
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::X11EventInjector>());
#endif
  Cimmerian::Visual::VisualTestRunSummary visualSummary =
      visualRunner.RunAll(&Cimmerian::Visual::VisualTestRegistry::GetInstance());

  return (summary.failed + visualSummary.failed == 0) ? 0 : 1;
}
