#pragma once

#include "../i-event-injector.hpp"

namespace Cimmerian::Visual {

// Injects via CGEventCreateMouseEvent / CGEventCreateKeyboardEvent +
// CGEventPost. Requires the process to hold the Accessibility permission
// (AXIsProcessTrusted()); without it, SetupError() says how to grant it and
// Inject() throws.
class MacOSEventInjector : public IEventInjector {
public:
  MacOSEventInjector();

  void Inject(const UIEvent& event) override;
  bool IsFunctional() const override { return this->setupError.empty(); }
  std::string SetupError() const override { return this->setupError; }

private:
  std::string setupError;
};

} // namespace Cimmerian::Visual
