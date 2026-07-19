#include "cimmerian/visual/visual-test-registry.hpp"
#include <cstring>

namespace Cimmerian::Visual {

static VisualTestGroup* FindGroup(VisualTestGroup* parent, const char* groupName)
{
  for (std::size_t i = 0; i < parent->GetChildCount(); ++i) {
    VisualTestGroup* child = parent->GetChild(i);
    if (std::strcmp(child->GetName(), groupName) == 0) {
      return child;
    }
  }
  return nullptr;
}

VisualTestRegistry::VisualTestRegistry()
    : root(std::make_unique<VisualTestGroup>("ROOT", nullptr, nullptr))
{
}

VisualTestRegistry& VisualTestRegistry::GetInstance()
{
  static VisualTestRegistry registry;
  return registry;
}

VisualTestGroup* VisualTestRegistry::GetChildGroup(
    VisualTestGroup* parent,
    const char* groupName,
    void* windowHandle
)
{
  if (VisualTestGroup* existing = FindGroup(parent, groupName)) {
    return existing;
  }
  auto group = std::make_unique<VisualTestGroup>(groupName, windowHandle, parent);
  VisualTestGroup* groupPtr = group.get();
  parent->AddChildGroup(std::move(group));
  return groupPtr;
}

std::string BuildVisualGroupPath(const VisualTestGroup* group)
{
  std::vector<const char*> names;
  for (const VisualTestGroup* current = group; current != nullptr; current = current->GetParent()) {
    if (std::strcmp(current->GetName(), "ROOT") == 0) {
      break;
    }
    names.push_back(current->GetName());
  }

  std::string path;
  for (auto it = names.rbegin(); it != names.rend(); ++it) {
    if (!path.empty()) {
      path += " > ";
    }
    path += *it;
  }
  return path;
}

void VisualTestRegistry::RegisterTest(
    VisualTestGroup* group,
    const char* testName,
    VisualTestCase::VisualTestCaseFn fn
)
{
  group->GetTests().emplace_back(testName, std::move(fn));
}

} // namespace Cimmerian::Visual
