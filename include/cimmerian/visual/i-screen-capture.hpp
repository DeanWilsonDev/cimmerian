#pragma once

#include <memory>
#include "screenshot.hpp"

namespace Cimmerian::Visual {

class IScreenCapture {
public:
  virtual ~IScreenCapture() = default;
  virtual Screenshot Capture(void* windowHandle) = 0;
};

// Holds the IScreenCapture implementation the current process should use.
// Set by VisualTestRunner::SetCapture(); read by Screenshot::Capture() so
// the plain Screenshot struct doesn't need to know about platform code.
class ActiveScreenCapture {
public:
  static ActiveScreenCapture& GetInstance();

  ActiveScreenCapture(const ActiveScreenCapture&) = delete;
  ActiveScreenCapture& operator=(const ActiveScreenCapture&) = delete;

  void Set(std::shared_ptr<IScreenCapture> capture) { this->capture = std::move(capture); }
  IScreenCapture* Get() const { return this->capture.get(); }

private:
  ActiveScreenCapture() = default;
  std::shared_ptr<IScreenCapture> capture;
};

} // namespace Cimmerian::Visual
