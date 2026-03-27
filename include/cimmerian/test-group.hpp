#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "test-case.hpp"

class TestGroup {
public:
  using TestHookFn = std::function<void(void*)>;

  TestGroup(const char* name, TestGroup* parent = nullptr)
      : nameStorage(name)
      , name(nameStorage.c_str())
      , parent(parent)
      , hookUser(nullptr)
      , hookTeardown(nullptr)
  {
  }

  const char* GetName() const { return this->nameStorage.c_str(); }

  std::vector<TestCase>& GetTests() { return this->tests; }
  const std::vector<TestCase>& GetTests() const { return this->tests; }

  TestGroup* GetParent() { return this->parent; }
  const TestGroup* GetParent() const { return this->parent; }

  void AddChildGroup(std::unique_ptr<TestGroup> child)
  {
    this->childGroups.push_back(std::move(child));
  }

  size_t GetChildCount() const { return this->childGroups.size(); }

  TestGroup* GetChild(size_t index) const
  {
    return index < this->childGroups.size() ? this->childGroups[index].get() : nullptr;
  }

  void SetBeforeAll(TestHookFn hook) { this->beforeAll = hook; }
  void SetAfterAll(TestHookFn hook) { this->afterAll = hook; }
  void SetBeforeEach(TestHookFn hook) { this->beforeEach = hook; }
  void SetAfterEach(TestHookFn hook) { this->afterEach = hook; }

  void SetHookUser(void* user, void (*teardown)(void*) = nullptr)
  {
    this->hookUser = user;
    this->hookTeardown = teardown;
  }

  void ExecuteBeforeAll() const
  {
    if (this->beforeAll)
      this->beforeAll(this->hookUser);
  }
  void ExecuteAfterAll() const
  {
    if (this->afterAll)
      this->afterAll(this->hookUser);
  }
  void ExecuteBeforeEach() const
  {
    if (this->beforeEach)
      this->beforeEach(this->hookUser);
  }
  void ExecuteAfterEach() const
  {
    if (this->afterEach)
      this->afterEach(this->hookUser);
  }

  ~TestGroup()
  {
    if (this->hookTeardown && this->hookUser) {
      this->hookTeardown(this->hookUser);
    }
  }

private:
  std::string nameStorage;
  const char* name;
  std::vector<TestCase> tests;
  TestGroup* parent;
  std::vector<std::unique_ptr<TestGroup>> childGroups;

  TestHookFn beforeAll;
  TestHookFn afterAll;
  TestHookFn beforeEach;
  TestHookFn afterEach;

  void* hookUser;
  void (*hookTeardown)(void*);
};

const char* CheckGroupName(const char* groupName);
