#include "cimmerian/snapshot/atomic-file-write.hpp"
#include <format>
#include <fstream>
#include <random>
#include <system_error>

namespace Cimmerian::Snapshot {

namespace fs = std::filesystem;

// Processes writing the same file at once each need their own temporary file.
static std::string RandomSuffix()
{
  std::random_device random;
  return std::format("{:08x}{:08x}", random(), random());
}

bool WriteFileAtomically(const fs::path& path, const std::string& contents, std::string* error)
{
  std::error_code errorCode;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), errorCode);
    if (errorCode) {
      *error = std::format("can't create {}: {}", path.parent_path().string(), errorCode.message());
      return false;
    }
  }

  // Next to path, because a rename is only atomic within one filesystem.
  fs::path tempPath = path;
  tempPath += ".tmp-" + RandomSuffix();

  std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
  out << contents;
  out.close();
  if (!out) {
    fs::remove(tempPath, errorCode);
    *error = std::format("can't write {}", tempPath.string());
    return false;
  }

  fs::rename(tempPath, path, errorCode);
  if (errorCode) {
    *error = std::format("can't replace {}: {}", path.string(), errorCode.message());
    fs::remove(tempPath, errorCode);
    return false;
  }
  return true;
}

} // namespace Cimmerian::Snapshot
