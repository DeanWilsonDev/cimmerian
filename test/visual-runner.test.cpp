#include "cimmerian/ansi-formatter.hpp"
#include "cimmerian/test.hpp"
#include "cimmerian/visual.hpp"
#include "support/scratch-dir.hpp"
#include "support/scratch-run.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>

#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) ||   \
    defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
#include <cstdlib>
#include <unistd.h>
#endif

// How VisualTestRunner reports a failing visual test. These need no window or
// screen access: the capture is a fake that returns a solid square, and each
// tree runs on a runner of its own, reporting to a temp file.

namespace {

namespace fs = std::filesystem;

using Cimmerian::Visual::DiffOptions;
using Cimmerian::Visual::Screenshot;
using Cimmerian::Visual::VisualRunMode;
using Cimmerian::Visual::VisualTestRegistry;
using Cimmerian::Visual::VisualTestRunSummary;
using CimmerianTestSupport::ScratchDir;

// A 4x4 square of one shade of red, whatever the window.
class SolidCapture : public Cimmerian::Visual::IScreenCapture {
public:
  explicit SolidCapture(std::uint8_t red)
      : red(red)
  {
  }

  Screenshot Capture(void*) override
  {
    Screenshot screenshot;
    screenshot.width = 4;
    screenshot.height = 4;
    screenshot.pixels.assign(4 * 4 * 4, 255);
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
      screenshot.pixels[pixel * 4 + 1] = 0;
      screenshot.pixels[pixel * 4 + 2] = 0;
      screenshot.pixels[pixel * 4] = this->red;
    }
    return screenshot;
  }

private:
  std::uint8_t red;
};

struct VisualRun {
  VisualTestRunSummary summary;
  // The report, without colours, with the snapshot root as <root> and line
  // numbers as <line>.
  std::string output;
};

// Can't inject anything, the way a platform backend can't without a
// display or a permission.
class BrokenInjector : public Cimmerian::Visual::IEventInjector {
public:
  void Inject(const Cimmerian::Visual::UIEvent&) override { this->injected = true; }
  bool IsFunctional() const override { return false; }
  std::string SetupError() const override { return "BrokenInjector: no input device"; }

  bool injected = false;
};

// One group, "Screens", with one test running body, capturing a square of
// the given red.
VisualRun RunVisualScratch(
    const fs::path& snapshotRoot,
    const std::function<void()>& body,
    std::uint8_t red = 0,
    VisualRunMode mode = VisualRunMode::Verify,
    std::shared_ptr<Cimmerian::Visual::IEventInjector> injector = nullptr
)
{
  VisualTestRegistry registry;
  registry.RegisterTest(
      registry.GetChildGroup(registry.GetRootGroup(), "Screens", nullptr), "shows the title",
      [body](void*) { body(); }
  );

  VisualRun run;
  std::FILE* stream = std::tmpfile();
  {
    Cimmerian::Visual::VisualTestRunner runner;
    if (stream) {
      runner.SetOutput(stream);
    }
    // Its own filter, so a --filter given to this binary doesn't skip it.
    runner.SetFilter("Screens");
    runner.SetMode(mode);
    runner.SetSnapshotRoot(snapshotRoot.string());
    runner.SetCapture(std::make_shared<SolidCapture>(red));
    runner.SetInjector(injector);
    runner.RunGroup(registry.GetRootGroup(), &run.summary);
  }
  Cimmerian::Visual::ActiveScreenCapture::GetInstance().Set(nullptr);
  Cimmerian::Visual::ActiveEventInjector::GetInstance().Set(nullptr);

  if (stream) {
    run.output = Cimmerian::Ansi::AnsiFormatter::StripCodes(CimmerianTestSupport::ReadAll(stream));
    std::fclose(stream);
  }
  std::string root = snapshotRoot.string();
  for (std::size_t at = run.output.find(root); at != std::string::npos;
       at = run.output.find(root, at)) {
    run.output.replace(at, root.size(), "<root>");
  }
  run.output = std::regex_replace(run.output, std::regex(R"((\.cpp):\d+)"), "$1:<line>");
  return run;
}

void SnapshotTitle()
{
  ASSERT_SNAPSHOT("title");
}

void ClickThenFail()
{
  SEND((Cimmerian::Visual::MouseClickEvent{.x = 10, .y = 10, .button = 1}));
  ASSERT_TRUE(false);
}

} // namespace

DESCRIBE("Visual runner failures", {
  TEST("a snapshot that differs from its golden fails, with the images under its [FAIL] line", {
    const ScratchDir snapshots;
    const VisualRun first = RunVisualScratch(snapshots.path, SnapshotTitle, 0);
    ASSERT_EQUAL(first.output, std::string("[NEW]  [Screens] shows the title (golden captured)\n"));

    const VisualRun second = RunVisualScratch(snapshots.path, SnapshotTitle, 255);
    ASSERT_EQUAL(
        second.output,
        std::string(
            "[FAIL] [Screens] shows the title\n"
            "\n"
            "      Snapshot 'title' differs from its golden in 100.00% of pixels (allowed 0.00%):\n"
            "\n"
            "            Golden  <root>/screens/shows-the-title/title.golden.png\n"
            "            Actual  <root>/screens/shows-the-title/title.actual.png\n"
            "            Diff    <root>/screens/shows-the-title/title.diff.png\n"
            "\n"
            "            Run with --update-snapshots to accept the new image.\n"
            "\n"
            "      at visual-runner.test.cpp:<line>\n"
            "\n"
        )
    );
    ASSERT_EQUAL(second.summary.failed, 1);
    ASSERT_EQUAL(second.summary.passed, 0);
  });

  TEST("a snapshot that matches its golden passes", {
    const ScratchDir snapshots;
    RunVisualScratch(snapshots.path, SnapshotTitle, 40);
    const VisualRun run = RunVisualScratch(snapshots.path, SnapshotTitle, 40);
    ASSERT_EQUAL(run.output, std::string("[PASS] [Screens] shows the title\n"));
    ASSERT_EQUAL(run.summary.passed, 1);
  });

  TEST("Review mode leaves a differing snapshot to its report instead of failing the test", {
    const ScratchDir snapshots;
    RunVisualScratch(snapshots.path, SnapshotTitle, 0);
    const VisualRun run =
        RunVisualScratch(snapshots.path, SnapshotTitle, 255, VisualRunMode::Review);
    ASSERT_EQUAL(run.output, std::string("[PASS] [Screens] shows the title\n"));
    ASSERT_EQUAL(run.summary.failed, 0);
  });

  TEST("an ASSERT_* in a visual test fails that test, not the unit test that's running", {
    const ScratchDir snapshots;
    const VisualRun run = RunVisualScratch(snapshots.path, [] { ASSERT_TRUE(1 + 1 == 3); });
    ASSERT_EQUAL(
        run.output, std::string(
                        "[FAIL] [Screens] shows the title\n"
                        "\n"
                        "      ASSERT_TRUE failed: 1 + 1 == 3\n"
                        "\n"
                        "      at visual-runner.test.cpp:<line>\n"
                        "\n"
                    )
    );
    ASSERT_EQUAL(run.summary.failed, 1);
  });

  TEST("an exception escaping a visual test fails it in every mode", {
    const ScratchDir snapshots;
    const auto throwing = [] { throw std::runtime_error("no window called \"Main\""); };
    for (const VisualRunMode mode :
         {VisualRunMode::Verify, VisualRunMode::Review, VisualRunMode::Update}) {
      const VisualRun run = RunVisualScratch(snapshots.path, throwing, 0, mode);
      ASSERT_EQUAL(
          run.output, std::string(
                          "[FAIL] [Screens] shows the title\n"
                          "\n"
                          "      The test threw an exception:\n"
                          "\n"
                          "            no window called \"Main\"\n"
                          "\n"
                      )
      );
      ASSERT_EQUAL(run.summary.failed, 1);
    }
  });

  TEST("hands the fail handler and GetActive() back when it's done", {
    Cimmerian::ITestFailHandler* const handlerBefore =
        Cimmerian::TestFailHandlerRegistry::GetInstance().GetHandler();
    Cimmerian::Visual::VisualTestRunner* const activeBefore =
        Cimmerian::Visual::VisualTestRunner::GetActive();
    const ScratchDir snapshots;
    RunVisualScratch(snapshots.path, [] { ASSERT_TRUE(false); });
    ASSERT_TRUE(Cimmerian::TestFailHandlerRegistry::GetInstance().GetHandler() == handlerBefore);
    ASSERT_TRUE(Cimmerian::Visual::VisualTestRunner::GetActive() == activeBefore);
  });
});

DESCRIBE("Visual injector that can't inject", {
  TEST("SetInjector warns once, naming why", {
    const ScratchDir snapshots;
    const VisualRun run = RunVisualScratch(
        snapshots.path, SnapshotTitle, 0, VisualRunMode::Verify, std::make_shared<BrokenInjector>()
    );
    ASSERT_EQUAL(
        run.output,
        std::string(
            "[WARNING] VisualTestRunner: BrokenInjector: no input device. Visual tests that SEND() "
            "events will fail; the others still run.\n"
            "[NEW]  [Screens] shows the title (golden captured)\n"
        )
    );
  });

  TEST("SEND() fails its test with the reason, and the test stops there", {
    const ScratchDir snapshots;
    const auto injector = std::make_shared<BrokenInjector>();
    const VisualRun run =
        RunVisualScratch(snapshots.path, ClickThenFail, 0, VisualRunMode::Verify, injector);
    ASSERT_TRUE(run.output.ends_with(
        "[FAIL] [Screens] shows the title\n"
        "\n"
        "      SEND() can't inject events:\n"
        "\n"
        "            BrokenInjector: no input device\n"
        "\n"
        "      at visual-runner.test.cpp:<line>\n"
        "\n"
    ));
    ASSERT_FALSE(injector->injected);
    ASSERT_EQUAL(run.summary.failed, 1);
  });

  TEST("SEND() with no injector registered fails its test", {
    const ScratchDir snapshots;
    const VisualRun run = RunVisualScratch(snapshots.path, ClickThenFail);
    ASSERT_TRUE(
        run.output.find(
            "SEND() can't inject events:\n\n            no IEventInjector is "
            "registered (call VisualTestRunner::SetInjector first)\n"
        ) != std::string::npos
    );
    ASSERT_EQUAL(run.summary.failed, 1);
  });
});

#if defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)
DESCRIBE("Platform injector that can't inject", {
  TEST("MacOSEventInjector reports a missing Accessibility permission instead of throwing", {
    std::unique_ptr<Cimmerian::Visual::MacOSEventInjector> injector;
    ASSERT_NO_THROW(injector = std::make_unique<Cimmerian::Visual::MacOSEventInjector>());
    REQUIRE_TRUE(injector != nullptr);
    ASSERT_EQUAL(injector->IsFunctional(), injector->SetupError().empty());
  });
});
#endif

#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
DESCRIBE("Platform injector that can't inject", {
  TEST("X11EventInjector reports a missing display instead of throwing", {
    const char* display = std::getenv("DISPLAY");
    const std::string savedDisplay = display ? display : "";
    unsetenv("DISPLAY");
    std::unique_ptr<Cimmerian::Visual::X11EventInjector> injector;
    ASSERT_NO_THROW(injector = std::make_unique<Cimmerian::Visual::X11EventInjector>());
    if (display) {
      setenv("DISPLAY", savedDisplay.c_str(), 1);
    }
    REQUIRE_TRUE(injector != nullptr);
    ASSERT_EQUAL(
        injector->SetupError(),
        std::string("X11EventInjector: unable to open X display (is $DISPLAY set?)")
    );
    ASSERT_FALSE(injector->Probe());
    ASSERT_THROWS(
        injector->Inject(Cimmerian::Visual::MouseMoveEvent{.x = 1, .y = 1}), std::runtime_error
    );
  });
});
#endif

#if defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)
DESCRIBE("Platform injector that can't inject", {
  TEST("LinuxUinputEventInjector reports /dev/uinput it can't open instead of throwing", {
    if (access("/dev/uinput", W_OK) == 0) {
      return;
    }
    std::unique_ptr<Cimmerian::Visual::LinuxUinputEventInjector> injector;
    ASSERT_NO_THROW(injector = std::make_unique<Cimmerian::Visual::LinuxUinputEventInjector>());
    REQUIRE_TRUE(injector != nullptr);
    ASSERT_TRUE(
        injector->SetupError().starts_with("LinuxUinputEventInjector: unable to open /dev/uinput")
    );
    ASSERT_FALSE(injector->Probe());
  });
});
#endif
