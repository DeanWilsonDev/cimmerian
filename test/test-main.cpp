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
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) ||                   \
    defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
  visualRunner.SetCapture(std::make_shared<Cimmerian::Visual::X11ScreenCapture>());
#endif
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::X11EventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::LinuxUinputEventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::AutoLinuxEventInjector>());
#endif
  Cimmerian::Visual::VisualTestRunSummary visualSummary =
      visualRunner.RunAll(&Cimmerian::Visual::VisualTestRegistry::GetInstance());

  return (summary.failed + visualSummary.failed == 0) ? 0 : 1;
}
