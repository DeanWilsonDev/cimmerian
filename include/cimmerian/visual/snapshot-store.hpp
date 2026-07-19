#pragma once

#include <string>

namespace Cimmerian::Visual {

class SnapshotStore {
public:
  explicit SnapshotStore(const std::string& rootDir);

  std::string GoldenPath(const std::string& group, const std::string& test, const std::string& label)
      const;
  std::string ActualPath(const std::string& group, const std::string& test, const std::string& label)
      const;
  std::string DiffPath(const std::string& group, const std::string& test, const std::string& label)
      const;

  bool GoldenExists(const std::string& group, const std::string& test, const std::string& label) const;

  const std::string& RootDir() const { return this->rootDir; }

  static std::string Slugify(const std::string& value);

private:
  std::string rootDir;

  std::string BasePath(const std::string& group, const std::string& test, const std::string& label) const;
};

} // namespace Cimmerian::Visual
