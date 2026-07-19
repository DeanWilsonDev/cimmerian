#include "cimmerian/visual/snapshot-store.hpp"
#include <cctype>
#include <filesystem>

namespace Cimmerian::Visual {

namespace fs = std::filesystem;

SnapshotStore::SnapshotStore(const std::string& rootDir)
    : rootDir(rootDir.empty() ? "./snapshots" : rootDir)
{
}

std::string SnapshotStore::Slugify(const std::string& value)
{
  std::string slug;
  slug.reserve(value.size());
  for (const char c : value) {
    if (c == ' ') {
      slug += '-';
    }
    else {
      slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
  }
  return slug;
}

std::string SnapshotStore::BasePath(const std::string& group, const std::string& test, const std::string& label)
    const
{
  return this->rootDir + "/" + Slugify(group) + "/" + Slugify(test) + "/" + label;
}

std::string SnapshotStore::GoldenPath(const std::string& group, const std::string& test, const std::string& label)
    const
{
  return this->BasePath(group, test, label) + ".golden.png";
}

std::string SnapshotStore::ActualPath(const std::string& group, const std::string& test, const std::string& label)
    const
{
  return this->BasePath(group, test, label) + ".actual.png";
}

std::string SnapshotStore::DiffPath(const std::string& group, const std::string& test, const std::string& label)
    const
{
  return this->BasePath(group, test, label) + ".diff.png";
}

bool SnapshotStore::GoldenExists(const std::string& group, const std::string& test, const std::string& label) const
{
  return fs::exists(this->GoldenPath(group, test, label));
}

} // namespace Cimmerian::Visual
