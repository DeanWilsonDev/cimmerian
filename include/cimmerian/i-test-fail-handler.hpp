#pragma once

#include "test-fail-record.hpp"

namespace Cimmerian {

class ITestFailHandler {
public:
  virtual ~ITestFailHandler() = default;
  virtual void OnTestFail(const TestFailRecord& failure) = 0;
};
} // namespace Cimmerian
