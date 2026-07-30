#include "cimmerian/visual.hpp"
#include "support/scratch-window.hpp"
#include <chrono>

using namespace std::chrono_literals;

namespace {

// Stands in for the consumer's own router/view-model: NAVIGATE("...") below
// reaches this instead of clicking through a menu with SEND(). Real
// consumers would call into their own app's navigation API here (see
// docs/cimmerian_navigation_without_platform_input_proposal.md Proposal A).
CIMMERIAN_MAYBE_UNUSED static const bool _registerScratchNavigator = []() {
  Cimmerian::Visual::ActiveNavigationDriver::GetInstance().Set(CimmerianTest::NavigateScratchWindow);
  return true;
}();

} // namespace

// The scratch window is a real on-screen window belonging to whichever
// platform backend is active, so its screenshot is inherently
// platform-specific - an X11 window screenshot never pixel-matches a macOS
// one. Each platform gets its own VISUAL_DESCRIBE group (and its own golden
// snapshots under snapshots/scratch-window-<platform>/) instead of sharing
// one golden across platforms.
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) ||                   \
    defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)

VISUAL_DESCRIBE("Scratch Window X11", CimmerianTest::GetScratchWindowHandle(), {
  VISUAL_TEST("looks the same between runs", {
    WAIT(20ms);
    ASSERT_SNAPSHOT("initial-state", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });

  VISUAL_TEST("reaches a screen via NAVIGATE instead of SEND", {
    NAVIGATE("red");
    WAIT(20ms);
    ASSERT_SNAPSHOT("red-screen", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });
});

#elif defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)

VISUAL_DESCRIBE("Scratch Window macOS", CimmerianTest::GetScratchWindowHandle(), {
  VISUAL_TEST("looks the same between runs", {
    WAIT(20ms);
    ASSERT_SNAPSHOT("initial-state", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });

  VISUAL_TEST("reaches a screen via NAVIGATE instead of SEND", {
    NAVIGATE("red");
    WAIT(20ms);
    ASSERT_SNAPSHOT("red-screen", Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
  });
});

#endif

// Stands in for the consumer's MountComponent<T> (Proposal B) - constructs
// a fresh, dedicated host window per test rather than reusing the shared
// scratch window above, and hands ASSERT_SNAPSHOT that window's handle
// directly instead of relying on a VISUAL_DESCRIBE-level one. Split by
// platform for the same reason as the Scratch Window groups above: the
// component host's screenshot is platform-specific, so X11 and macOS must
// never share a golden.
#if defined(CIMMERIAN_VISUAL_PLATFORM_X11) || defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT) ||                   \
    defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)

VISUAL_DESCRIBE_COMPONENT("Scratch Component X11", {
  VISUAL_TEST("default background", {
    CimmerianTest::ScratchComponentHost host = CimmerianTest::MountScratchComponent(0x33AA55);
    WAIT(20ms);
    ASSERT_SNAPSHOT("default", host.WindowHandle(), Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
    CimmerianTest::UnmountScratchComponent(host);
  });
});

#elif defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)

VISUAL_DESCRIBE_COMPONENT("Scratch Component macOS", {
  VISUAL_TEST("default background", {
    CimmerianTest::ScratchComponentHost host = CimmerianTest::MountScratchComponent(0x33AA55);
    WAIT(20ms);
    ASSERT_SNAPSHOT("default", host.WindowHandle(), Cimmerian::Visual::DiffOptions {.maxDifferencePercent = 1.0f});
    CimmerianTest::UnmountScratchComponent(host);
  });
});

#endif
