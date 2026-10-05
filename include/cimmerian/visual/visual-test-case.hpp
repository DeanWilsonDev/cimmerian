#pragma once

#include <functional>
#include <string>
#include "../test-mode.hpp"

namespace Cimmerian::Visual {

class VisualTestCase {
public:
  using VisualTestCaseFn = std::function<void(void*)>;

  VisualTestCase(
      const char* name,
      VisualTestCaseFn fn,
      void* user = nullptr,
      TestMode mode = TestMode::Normal
  )
      : nameStorage(name)
      , fn(std::move(fn))
      , user(user)
      , mode(mode)
  {
  }

  void Run() const
  {
    if (this->fn) {
      this->fn(this->user);
    }
  }

  const char* GetName() const { return this->nameStorage.c_str(); }
  TestMode GetMode() const { return this->mode; }

private:
  std::string nameStorage;
  VisualTestCaseFn fn;
  void* user;
  TestMode mode;
};

} // namespace Cimmerian::Visual
