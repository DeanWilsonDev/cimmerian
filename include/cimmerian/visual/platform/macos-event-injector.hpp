#pragma once

#include "../i-event-injector.hpp"

namespace Cimmerian::Visual {

// Injects via CGEventCreateMouseEvent / CGEventCreateKeyboardEvent +
// CGEventPost. Requires the process to hold the Accessibility permission
// (AXIsProcessTrusted()); the constructor throws if it does not.
class MacOSEventInjector : public IEventInjector {
public:
  MacOSEventInjector();

  void Inject(const UIEvent& event) override;
};

} // namespace Cimmerian::Visual
