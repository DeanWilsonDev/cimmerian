#include "cimmerian/test-group.hpp"
#include <vector>
#include <cstring>

static const char* DEFAULT_GROUP_NAME = "Ungrouped";

const char* CheckGroupName(const char* groupName)
{
    return groupName ? groupName : DEFAULT_GROUP_NAME;
}

std::string BuildGroupPath(const TestGroup* group)
{
  std::vector<const char*> names;
  for (const TestGroup* current = group; current != nullptr; current = current->GetParent()) {
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
