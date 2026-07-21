#pragma once

#include <functional>
#include <string>

namespace Cimmerian::Visual {

// Consumer-supplied hook that puts the app under test into a named
// screen/state directly (e.g. by calling the app's own router/view-model
// API in-process, or sending it an out-of-band IPC/debug command) rather
// than by simulating the clicks a real user would make to get there.
// Opaque string key - Cimmerian doesn't know or care what the consumer's
// screens are called; it's whatever the consumer's own navigation system
// uses (route name, enum, panel id, ...).
using NavigateFn = std::function<void(const std::string& screenKey)>;

// Holds the NavigateFn the current process should use. Set once by the
// consumer's test-main (mirrors ActiveEventInjector/ActiveScreenCapture);
// read by the NAVIGATE macro.
class ActiveNavigationDriver {
public:
  static ActiveNavigationDriver& GetInstance();

  ActiveNavigationDriver(const ActiveNavigationDriver&) = delete;
  ActiveNavigationDriver& operator=(const ActiveNavigationDriver&) = delete;

  void Set(NavigateFn fn) { this->navigate = std::move(fn); }

  // Throws if no navigator was registered - this is opt-in, not a silent
  // no-op, since a NAVIGATE() that quietly does nothing would let a
  // subsequent ASSERT_SNAPSHOT pass against whatever screen happened to
  // already be showing rather than the one the test asked for.
  void NavigateTo(const std::string& screenKey);

private:
  ActiveNavigationDriver() = default;
  NavigateFn navigate;
};

} // namespace Cimmerian::Visual
