#pragma once

#include "i-test-fail-handler.hpp"
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

  void NotifyTestFail(const TestFailRecord& failure)
  {
    if (this->activeHandler) {
      this->activeHandler->OnTestFail(failure);
      return;
    }

    std::fprintf(
        stderr, "%s:%d: ASSERTION FAILED (no runner): %s\n", failure.file.c_str(), failure.line,
        failure.message.c_str()
    );
    for (const std::string& detailLine : failure.details) {
      std::fprintf(stderr, "    %s\n", detailLine.c_str());
    }
  }

  // For a caller holding one preformatted message: its first line becomes the
  // record's message and any lines after it the details.
  void NotifyTestFail(const char* file, int line, std::string_view message)
  {
    std::vector<std::string> lines = SplitLines(message);
    std::string headline;
    if (!lines.empty()) {
      headline = std::move(lines.front());
      lines.erase(lines.begin());
    }
    this->NotifyTestFail(
        {.file = file, .line = line, .message = std::move(headline), .details = std::move(lines)}
    );
  }

  TestFailHandlerRegistry(const TestFailHandlerRegistry&) = delete;
  TestFailHandlerRegistry& operator=(const TestFailHandlerRegistry&) = delete;

private:
  TestFailHandlerRegistry() = default;
  ITestFailHandler* activeHandler = nullptr;
};

} // namespace Cimmerian
