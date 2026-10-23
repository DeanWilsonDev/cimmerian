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

  const std::string x11Error = this->x11Injector.SetupError();
  if (x11Error.empty() && this->x11Injector.Probe()) {
    return true;
  }

  if (x11Error.empty()) {
    TEST_LOG_WARN(
        "AutoLinuxEventInjector: XTEST reports success but doesn't reach the "
        "real input pipeline on this compositor - falling back to "
        "LinuxUinputEventInjector (/dev/uinput)."
    );
  }
  else {
    TEST_LOG_WARN(
        "AutoLinuxEventInjector: {} - falling back to LinuxUinputEventInjector (/dev/uinput).",
        x11Error
    );
  }

  auto fallback = std::make_unique<LinuxUinputEventInjector>();
  const std::string uinputError = fallback->SetupError();
  if (!uinputError.empty()) {
    if (x11Error.empty()) {
      TEST_LOG_WARN(
          "AutoLinuxEventInjector: uinput fallback unavailable ({}) - staying "
          "on XTEST despite it being non-functional; SEND() in visual tests "
          "will silently no-op.",
          uinputError
      );
    }
    else {
      this->bothFailed = x11Error + "; and the fallback " + uinputError;
    }
    return false;
  }

  this->uinputFallback = std::move(fallback);
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

std::string AutoLinuxEventInjector::SetupError() const
{
  if (!this->bothFailed.empty()) {
    return this->bothFailed;
  }
  return this->usingFallback ? this->uinputFallback->SetupError() : this->x11Injector.SetupError();
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
  if (!this->bothFailed.empty()) {
    throw std::runtime_error(this->bothFailed);
  }
  if (this->usingFallback) {
    this->uinputFallback->Inject(event);
  }
  else {
    this->x11Injector.Inject(event);
  }
}

} // namespace Cimmerian::Visual
