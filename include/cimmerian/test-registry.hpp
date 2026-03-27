#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "test-case.hpp"
#include "test-group.hpp"

namespace Cimmerian {

class TestRegistry {
public:
  static TestRegistry& GetInstance();

  TestRegistry();
  ~TestRegistry() = default;

  TestRegistry(const TestRegistry&) = delete;
  TestRegistry& operator=(const TestRegistry&) = delete;

  TestGroup* GetRootGroup() { return this->root.get(); }
  const TestGroup* GetRootGroup() const { return this->root.get(); }

  TestGroup* FindOrCreateGroup(const char* groupName);

  TestGroup* GetChildGroup(TestGroup* parent, const char* groupName);
  TestGroup* AddChildToGroup(TestGroup* parent, const char* groupName);

  void RegisterTest(
      TestGroup* group,
      const char* testName,
      TestCase::TestCaseFn fn,
      void* user = nullptr,
      TestCase::TestTeardownFn teardownFn = nullptr
  );

  void SetBeforeAll(
      TestGroup* group,
      TestGroup::TestHookFn fn,
      void* hookUser = nullptr,
      void (*hookTeardown)(void*) = nullptr
  );

  void SetAfterAll(
      TestGroup* group,
      TestGroup::TestHookFn fn,
      void* hookUser = nullptr,
      void (*hookTeardown)(void*) = nullptr
  );

  void SetBeforeEach(
      TestGroup* group,
      TestGroup::TestHookFn fn,
      void* hookUser = nullptr,
      void (*hookTeardown)(void*) = nullptr
  );

  void SetAfterEach(
      TestGroup* group,
      TestGroup::TestHookFn fn,
      void* hookUser = nullptr,
      void (*hookTeardown)(void*) = nullptr
  );

private:
  std::unique_ptr<TestGroup> root;
  std::unordered_map<std::string, std::unique_ptr<char[]>> stringPool;

  char* StoreString(const char* str);
};
} // namespace Cimmerian
