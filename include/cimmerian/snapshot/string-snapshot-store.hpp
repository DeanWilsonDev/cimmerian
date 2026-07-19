#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "snapshot-key.hpp"

namespace Cimmerian::Snapshot {

// In-memory representation of a single .snap file: an ordered list of
// key/content entries, so re-writing preserves insertion order.
class SnapFile {
public:
  std::optional<std::string> Find(const std::string& key) const;
  void Set(const std::string& key, const std::string& value);

  const std::vector<std::pair<std::string, std::string>>& Entries() const { return this->entries; }

  static SnapFile Parse(const std::string& fileContents);
  std::string Serialize() const;

private:
  std::vector<std::pair<std::string, std::string>> entries;
  std::unordered_map<std::string, std::size_t> indexByKey;
};

class StringSnapshotStore {
public:
  static StringSnapshotStore& GetInstance();

  explicit StringSnapshotStore(const std::string& snapshotRootDir = "");

  StringSnapshotStore(const StringSnapshotStore&) = delete;
  StringSnapshotStore& operator=(const StringSnapshotStore&) = delete;

  std::optional<std::string> Load(const SnapshotKey& key);
  void Save(const SnapshotKey& key, const std::string& value);
  void Flush();

private:
  std::string snapshotRootDir;
  std::unordered_map<std::string, SnapFile> loadedFiles;

  std::string ResolveSnapFilePath(const SnapshotKey& key) const;
  SnapFile& GetOrLoadFile(const std::string& path);
};

} // namespace Cimmerian::Snapshot
