#include "cimmerian/visual/navigation-driver.hpp"
#include <stdexcept>
#include "cimmerian/test-log.hpp"

namespace Cimmerian::Visual {

ActiveNavigationDriver& ActiveNavigationDriver::GetInstance()
{
  static ActiveNavigationDriver instance;
  return instance;
}

void ActiveNavigationDriver::NavigateTo(const std::string& screenKey)
{
  if (!this->navigate) {
    TEST_LOG_ERROR(
        "NAVIGATE(\"{}\"): no NavigateFn registered (call "
        "ActiveNavigationDriver::GetInstance().Set(...) from test-main before "
        "running visual tests that use NAVIGATE)",
        screenKey
    );
    throw std::runtime_error(
        "NAVIGATE(\"" + screenKey + "\"): no NavigateFn registered"
    );
  }
  this->navigate(screenKey);
}

} // namespace Cimmerian::Visual
