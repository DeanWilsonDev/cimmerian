#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "snapshot-key.hpp"

namespace Cimmerian::Snapshot {

class HashSnapFile {
public:
  std::optional<std::string> Find(const std::string& key) const;
  void Set(const std::string& key, const std::string& hexHash);

  const std::vector<std::pair<std::string, std::string>>& Entries() const { return this->entries; }

  static HashSnapFile Parse(const std::string& fileContents);
  std::string Serialize() const;

private:
  std::vector<std::pair<std::string, std::string>> entries;
  std::unordered_map<std::string, std::size_t> indexByKey;
};

class HashSnapshotStore {
public:
  static HashSnapshotStore& GetInstance();

  explicit HashSnapshotStore(const std::string& snapshotRootDir = "");

  HashSnapshotStore(const HashSnapshotStore&) = delete;
  HashSnapshotStore& operator=(const HashSnapshotStore&) = delete;

  std::optional<std::string> Load(const SnapshotKey& key);
  void Save(const SnapshotKey& key, const std::string& hexHash);
  void Flush();

private:
  std::string snapshotRootDir;
  std::unordered_map<std::string, HashSnapFile> loadedFiles;

  std::string ResolveSnapHashFilePath(const SnapshotKey& key) const;
  HashSnapFile& GetOrLoadFile(const std::string& path);
};

} // namespace Cimmerian::Snapshot
