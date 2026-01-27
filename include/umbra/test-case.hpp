#pragma once

#include <functional>

class TestCase {
public:
  using TestCaseFn = std::function<void(void*)>;
  using TestTeardownFn = std::function<void(void*)>;

  TestCase(const char* name, TestCaseFn fn, void* user = nullptr, TestTeardownFn teardown = nullptr)
      : nameStorage(name),
        name(nameStorage.c_str())
      , fn(fn)
      , user(user)
      , teardown(teardown)
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

private:
  std::string nameStorage;
  const char* name;
  TestCaseFn fn;
  void* user;
  TestTeardownFn teardown;
};
