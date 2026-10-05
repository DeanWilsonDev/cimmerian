#pragma once

#include <functional>
#include <string>
#include "test-mode.hpp"

class TestCase {
public:
  using TestCaseFn = std::function<void(void*)>;
  using TestTeardownFn = std::function<void(void*)>;

  TestCase(
      const char* name,
      TestCaseFn fn,
      void* user = nullptr,
      TestTeardownFn teardown = nullptr,
      Cimmerian::TestMode mode = Cimmerian::TestMode::Normal
  )
      : nameStorage(name),
        name(nameStorage.c_str())
      , fn(fn)
      , user(user)
      , teardown(teardown)
      , mode(mode)
  {
  }

  void Run() const
  {
    if (fn) {
      fn(user);
    }
  }

  void Teardown() const
  {
    if (teardown) {
      teardown(user);
    }
  }

  const char* GetName() const { return nameStorage.c_str(); }
  Cimmerian::TestMode GetMode() const { return mode; }

private:
  std::string nameStorage;
  const char* name;
  TestCaseFn fn;
  void* user;
  TestTeardownFn teardown;
  Cimmerian::TestMode mode;
};
