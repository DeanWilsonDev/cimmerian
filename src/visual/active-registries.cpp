#include "cimmerian/visual/i-event-injector.hpp"
#include "cimmerian/visual/i-screen-capture.hpp"

namespace Cimmerian::Visual {

ActiveScreenCapture& ActiveScreenCapture::GetInstance()
{
  static ActiveScreenCapture instance;
  return instance;
}

ActiveEventInjector& ActiveEventInjector::GetInstance()
{
  static ActiveEventInjector instance;
  return instance;
}

} // namespace Cimmerian::Visual
