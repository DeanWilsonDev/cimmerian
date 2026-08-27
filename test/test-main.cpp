#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
#include <cimmerian/snapshot/snapshot-run-mode.hpp>
#endif
#include <cimmerian/test-debug.hpp>
#include <cimmerian/test-runner.hpp>
#ifdef CIMMERIAN_ENABLE_VISUAL_TESTING
#include <cimmerian/visual.hpp>
#include <memory>
#endif

int main(int argc, char* argv[])
{
  Cimmerian::CheckDebug(argc, argv);
#ifdef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
  Cimmerian::Snapshot::SnapshotRunModeRegistry::GetInstance().ParseArgs(argc, argv);
#endif

  Cimmerian::TestRunner runner;
  Cimmerian::TestRunSummary summary = runner.RunAll(&Cimmerian::TestRegistry::GetInstance());

  int failed = summary.failed;

#ifdef CIMMERIAN_ENABLE_VISUAL_TESTING
  Cimmerian::Visual::VisualTestRunner visualRunner;
  visualRunner.ParseArgs(argc, argv);
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) ||                   \
    defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
  visualRunner.SetCapture(std::make_shared<Cimmerian::Visual::X11ScreenCapture>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)
  visualRunner.SetCapture(std::make_shared<Cimmerian::Visual::MacOSScreenCapture>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_WIN32)
  visualRunner.SetCapture(std::make_shared<Cimmerian::Visual::Win32ScreenCapture>());
#endif
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::X11EventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::LinuxUinputEventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::AutoLinuxEventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::MacOSEventInjector>());
#elif defined(CIMMERIAN_VISUAL_PLATFORM_WIN32)
  visualRunner.SetInjector(std::make_shared<Cimmerian::Visual::Win32EventInjector>());
#endif
  Cimmerian::Visual::VisualTestRunSummary visualSummary =
      visualRunner.RunAll(&Cimmerian::Visual::VisualTestRegistry::GetInstance());
  failed += visualSummary.failed;
#endif

  return (failed == 0) ? 0 : 1;
}
