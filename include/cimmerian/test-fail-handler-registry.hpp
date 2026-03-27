#pragma once

#include "i-test-fail-handler.hpp"
#include <iostream>

namespace Cimmerian {

class TestFailHandlerRegistry {

public:
  static TestFailHandlerRegistry& GetInstance()
  {
    static TestFailHandlerRegistry singletonInstance;
    return singletonInstance;
  }

  void RegisterHandler(ITestFailHandler* handlerToRegister)
  {
    this->activeHandler = handlerToRegister;
  }

  void NotifyTestFail(const char* file, int line, const char* msg)
  {
    if (this->activeHandler) {
      this->activeHandler->OnTestFail(file, line, msg);
    }
    else {
      std::fprintf(stderr, "%s:%d: ASSERTION FAILED (no runner): %s\n", file, line, msg);
    }
  }

  TestFailHandlerRegistry(const TestFailHandlerRegistry&) = delete;
  TestFailHandlerRegistry& operator=(const TestFailHandlerRegistry&) = delete;

private:
  TestFailHandlerRegistry() = default;
  ITestFailHandler* activeHandler = nullptr;
};

} // namespace Cimmerian
