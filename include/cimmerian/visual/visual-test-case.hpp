#pragma once

#include <functional>
#include <string>

namespace Cimmerian::Visual {

class VisualTestCase {
public:
  using VisualTestCaseFn = std::function<void(void*)>;

  VisualTestCase(const char* name, VisualTestCaseFn fn, void* user = nullptr)
      : nameStorage(name)
      , fn(std::move(fn))
      , user(user)
  {
  }

  void Run() const
  {
    if (this->fn) {
      this->fn(this->user);
    }
  }

  const char* GetName() const { return this->nameStorage.c_str(); }

private:
  std::string nameStorage;
  VisualTestCaseFn fn;
  void* user;
};

} // namespace Cimmerian::Visual
