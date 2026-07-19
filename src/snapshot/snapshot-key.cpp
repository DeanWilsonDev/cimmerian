#include "cimmerian/snapshot/snapshot-key.hpp"

namespace Cimmerian::Snapshot {

std::string SnapshotKeyToString(const SnapshotKey& key)
{
  if (key.scopePath.empty()) {
    return key.label;
  }
  return key.scopePath + " > " + key.label;
}

std::string SnapshotDirForFile(const std::string& filePath)
{
  std::string dir;
  const std::size_t lastSlash = filePath.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    dir = filePath.substr(0, lastSlash);
  }
  return dir.empty() ? "__snapshots__" : dir + "/__snapshots__";
}

std::string SnapshotStemForFile(const std::string& filePath)
{
  const std::size_t lastSlash = filePath.find_last_of("/\\");
  std::string fileName = (lastSlash == std::string::npos) ? filePath : filePath.substr(lastSlash + 1);

  const std::size_t lastDot = fileName.find_last_of('.');
  if (lastDot != std::string::npos && lastDot != 0) {
    fileName = fileName.substr(0, lastDot);
  }
  return fileName;
}

} // namespace Cimmerian::Snapshot
