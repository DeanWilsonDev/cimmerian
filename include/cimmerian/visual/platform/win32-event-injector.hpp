#pragma once

#include "../i-event-injector.hpp"

namespace Cimmerian::Visual {

// Injects via SendInput. Requires no extra permissions when the target
// window belongs to the same session/process.
class Win32EventInjector : public IEventInjector {
public:
  Win32EventInjector() = default;

  void Inject(const UIEvent& event) override;
};

} // namespace Cimmerian::Visual
