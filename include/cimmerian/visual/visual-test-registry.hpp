#pragma once

#include <memory>
#include <string>
#include <vector>
#include "visual-test-case.hpp"

namespace Cimmerian::Visual {

class VisualTestGroup {
public:
  VisualTestGroup(const char* name, void* windowHandle, VisualTestGroup* parent = nullptr)
      : nameStorage(name)
      , windowHandle(windowHandle)
      , parent(parent)
  {
  }

  const char* GetName() const { return this->nameStorage.c_str(); }

  // A group with no window handle of its own inherits its parent's.
  void* GetWindowHandle() const
  {
    if (this->windowHandle != nullptr) {
      return this->windowHandle;
    }
    return this->parent ? this->parent->GetWindowHandle() : nullptr;
  }

  std::vector<VisualTestCase>& GetTests() { return this->tests; }
  const std::vector<VisualTestCase>& GetTests() const { return this->tests; }

  VisualTestGroup* GetParent() { return this->parent; }
  const VisualTestGroup* GetParent() const { return this->parent; }

  void AddChildGroup(std::unique_ptr<VisualTestGroup> child)
  {
    this->childGroups.push_back(std::move(child));
  }

  std::size_t GetChildCount() const { return this->childGroups.size(); }

  VisualTestGroup* GetChild(std::size_t index) const
  {
    return index < this->childGroups.size() ? this->childGroups[index].get() : nullptr;
  }

private:
  std::string nameStorage;
  void* windowHandle;
  std::vector<VisualTestCase> tests;
  VisualTestGroup* parent;
  std::vector<std::unique_ptr<VisualTestGroup>> childGroups;
};

class VisualTestRegistry {
public:
  static VisualTestRegistry& GetInstance();

  VisualTestRegistry();
  ~VisualTestRegistry() = default;

  VisualTestRegistry(const VisualTestRegistry&) = delete;
  VisualTestRegistry& operator=(const VisualTestRegistry&) = delete;

  VisualTestGroup* GetRootGroup() { return this->root.get(); }
  const VisualTestGroup* GetRootGroup() const { return this->root.get(); }

  VisualTestGroup* GetChildGroup(VisualTestGroup* parent, const char* groupName, void* windowHandle);

  void RegisterTest(VisualTestGroup* group, const char* testName, VisualTestCase::VisualTestCaseFn fn);

private:
  std::unique_ptr<VisualTestGroup> root;
};

// Builds "Group > Subgroup > ..." from a group up to (excluding) the ROOT group.
std::string BuildVisualGroupPath(const VisualTestGroup* group);

} // namespace Cimmerian::Visual
