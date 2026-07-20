#pragma once

#include <memory>
#include "ui-event.hpp"

namespace Cimmerian::Visual {

class IEventInjector {
public:
  virtual ~IEventInjector() = default;
  virtual void Inject(const UIEvent& event) = 0;

  // Whether this injector's events are actually expected to reach the
  // target. Most platform backends talk to an OS-level input API that either
  // works or throws at construction, so the default is true. X11EventInjector
  // overrides this: XTEST can report success while a Wayland compositor
  // silently drops the synthesized input (see
  // docs/cimmerian_wayland_xtest_injection_gap.md), so it runs a self-check
  // and reports the real answer here instead of a blind "yes".
  virtual bool IsFunctional() const { return true; }
};

// Holds the IEventInjector implementation the current process should use.
// Set by VisualTestRunner::SetInjector(); read by the SEND / WAIT macros.
class ActiveEventInjector {
public:
  static ActiveEventInjector& GetInstance();

  ActiveEventInjector(const ActiveEventInjector&) = delete;
  ActiveEventInjector& operator=(const ActiveEventInjector&) = delete;

  void Set(std::shared_ptr<IEventInjector> injector) { this->injector = std::move(injector); }
  IEventInjector* Get() const { return this->injector.get(); }

private:
  ActiveEventInjector() = default;
  std::shared_ptr<IEventInjector> injector;
};

} // namespace Cimmerian::Visual
