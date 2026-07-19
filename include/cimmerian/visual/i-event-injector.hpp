#pragma once

#include <memory>
#include "ui-event.hpp"

namespace Cimmerian::Visual {

class IEventInjector {
public:
  virtual ~IEventInjector() = default;
  virtual void Inject(const UIEvent& event) = 0;
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
