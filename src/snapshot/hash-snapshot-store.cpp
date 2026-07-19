#include "cimmerian/snapshot/hash-snapshot-store.hpp"
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Cimmerian::Snapshot {

namespace fs = std::filesystem;

std::optional<std::string> HashSnapFile::Find(const std::string& key) const
{
  auto it = this->indexByKey.find(key);
  if (it == this->indexByKey.end()) {
    return std::nullopt;
  }
  return this->entries[it->second].second;
}

void HashSnapFile::Set(const std::string& key, const std::string& hexHash)
{
  auto it = this->indexByKey.find(key);
  if (it != this->indexByKey.end()) {
    this->entries[it->second].second = hexHash;
    return;
  }
  this->indexByKey[key] = this->entries.size();
  this->entries.emplace_back(key, hexHash);
}

HashSnapFile HashSnapFile::Parse(const std::string& fileContents)
{
  HashSnapFile file;
  std::istringstream stream(fileContents);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) {
      continue;
    }
    const std::size_t sep = line.rfind(": ");
    if (sep == std::string::npos) {
      continue;
    }
    file.Set(line.substr(0, sep), line.substr(sep + 2));
  }
  return file;
}

std::string HashSnapFile::Serialize() const
{
  std::string out;
  for (const auto& [key, hash] : this->entries) {
    out += key;
    out += ": ";
    out += hash;
    out += '\n';
  }
  return out;
}

HashSnapshotStore& HashSnapshotStore::GetInstance()
{
  static HashSnapshotStore instance;
  return instance;
}

HashSnapshotStore::HashSnapshotStore(const std::string& snapshotRootDir)
    : snapshotRootDir(snapshotRootDir)
{
}

std::string HashSnapshotStore::ResolveSnapHashFilePath(const SnapshotKey& key) const
{
  std::string root = this->snapshotRootDir;
  if (root.empty()) {
    if (const auto& override_ = SnapshotRunModeRegistry::GetInstance().GetSnapshotRootOverride();
        override_.has_value()) {
      root = *override_;
    }
  }
  if (root.empty()) {
    root = SnapshotDirForFile(key.filePath);
  }
  return root + "/" + SnapshotStemForFile(key.filePath) + ".snaphash";
}

HashSnapFile& HashSnapshotStore::GetOrLoadFile(const std::string& path)
{
  auto it = this->loadedFiles.find(path);
  if (it != this->loadedFiles.end()) {
    return it->second;
  }

  HashSnapFile file;
  std::ifstream in(path, std::ios::binary);
  if (in) {
    std::ostringstream buffer;
    buffer << in.rdbuf();
    file = HashSnapFile::Parse(buffer.str());
  }

  auto [inserted, _] = this->loadedFiles.emplace(path, std::move(file));
  return inserted->second;
}

std::optional<std::string> HashSnapshotStore::Load(const SnapshotKey& key)
{
  const std::string path = this->ResolveSnapHashFilePath(key);
  HashSnapFile& file = this->GetOrLoadFile(path);
  return file.Find(SnapshotKeyToString(key));
}

void HashSnapshotStore::Save(const SnapshotKey& key, const std::string& hexHash)
{
  const std::string path = this->ResolveSnapHashFilePath(key);
  HashSnapFile& file = this->GetOrLoadFile(path);
  file.Set(SnapshotKeyToString(key), hexHash);
}

void HashSnapshotStore::Flush()
{
  for (const auto& [path, file] : this->loadedFiles) {
    fs::path fsPath(path);
    if (fsPath.has_parent_path()) {
      fs::create_directories(fsPath.parent_path());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << file.Serialize();
  }
}

} // namespace Cimmerian::Snapshot
