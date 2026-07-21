#include "cimmerian/visual/platform/auto-linux-event-injector.hpp"
#include <stdexcept>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

AutoLinuxEventInjector::AutoLinuxEventInjector() = default;

bool AutoLinuxEventInjector::Probe()
{
  if (this->usingFallback) {
    return this->uinputFallback->IsFunctional();
  }

  if (this->x11Injector.Probe()) {
    return true;
  }

  TEST_LOG_WARN(
      "AutoLinuxEventInjector: XTEST reports success but doesn't reach the "
      "real input pipeline on this compositor - falling back to "
      "LinuxUinputEventInjector (/dev/uinput)."
  );

  try {
    this->uinputFallback = std::make_unique<LinuxUinputEventInjector>();
    this->usingFallback = true;
    const bool uinputFunctional = this->uinputFallback->Probe();
    if (!uinputFunctional) {
      TEST_LOG_WARN(
          "AutoLinuxEventInjector: neither XTEST nor the /dev/uinput "
          "fallback reach the real input pipeline on this session - SEND() "
          "in visual tests will silently no-op (see "
          "docs/cimmerian_uinput_no_functional_check_gap.md)."
      );
    }
    return uinputFunctional;
  }
  catch (const std::exception& ex) {
    TEST_LOG_WARN(
        "AutoLinuxEventInjector: uinput fallback unavailable ({}) - staying "
        "on XTEST despite it being non-functional; SEND() in visual tests "
        "will silently no-op.",
        ex.what()
    );
    return false;
  }
}

bool AutoLinuxEventInjector::IsFunctional() const
{
  return this->usingFallback ? this->uinputFallback->IsFunctional() : this->x11Injector.IsFunctional();
}

bool AutoLinuxEventInjector::IsWindowFocused(void* windowHandle) const
{
  // LinuxUinputEventInjector has no notion of "window" (it's below both X11
  // and Wayland at the kernel input-device layer), so there's nothing
  // meaningful to check once the fallback is active.
  return this->usingFallback ? true : this->x11Injector.IsWindowFocused(windowHandle);
}

void AutoLinuxEventInjector::Inject(const UIEvent& event)
{
  if (this->usingFallback) {
    this->uinputFallback->Inject(event);
  }
  else {
    this->x11Injector.Inject(event);
  }
}

} // namespace Cimmerian::Visual
