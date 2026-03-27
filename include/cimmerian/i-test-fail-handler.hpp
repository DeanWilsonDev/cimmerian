#pragma once

namespace Cimmerian {

class ITestFailHandler {
public:
  virtual ~ITestFailHandler() = default;
  virtual void OnTestFail(const char* file, int line, const char* msg) = 0;
};
} // namespace Cimmerian
