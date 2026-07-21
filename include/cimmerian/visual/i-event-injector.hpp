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

  // Runs whatever deliberate self-test this backend needs to determine if
  // its events actually reach the target, updating what IsFunctional()
  // subsequently reports, and returns the result. Most backends have
  // nothing to probe, so the default just returns IsFunctional() as-is.
  // X11EventInjector overrides this with a real XQueryPointer probe that
  // visibly moves the system cursor - callers (VisualTestRunner::SetInjector)
  // invoke it explicitly, once, rather than it running as an unannounced
  // side effect of construction (see
  // docs/cimmerian_live_app_visual_testing_gap.md gap 3).
  virtual bool Probe() { return this->IsFunctional(); }

  // Best-effort check of whether windowHandle currently has real OS input
  // focus. Both provided Linux backends inject real, screen-absolute OS
  // input with no concept of "target window" - if focus shifts mid-test
  // (another app's notification, an alt-tab, WAIT()-adjacent user activity)
  // the next Inject() lands wherever focus actually is, not necessarily the
  // window under test (see docs/cimmerian_live_app_visual_testing_gap.md gap
  // 4). Default true (assume focused / not checkable), since a null handle
  // or a backend with no notion of "window" has no meaningful answer.
  virtual bool IsWindowFocused(void* windowHandle) const
  {
    (void)windowHandle;
    return true;
  }
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
