#pragma once

#include <string>

namespace Cimmerian::Snapshot {

struct SnapshotKey {
  std::string filePath;   // absolute path to the .test.cpp file
  std::string scopePath;  // "Group > Subgroup > ..."
  std::string label;      // the name passed to ASSERT_STRING_SNAPSHOT / etc.
};

// "Group > Subgroup > label"
std::string SnapshotKeyToString(const SnapshotKey& key);

// Derives the __snapshots__ directory adjacent to a test file, e.g.
// "test/my-feature.test.cpp" -> "test/__snapshots__"
std::string SnapshotDirForFile(const std::string& filePath);

// Derives the snapshot data file stem for a test file, e.g.
// "test/my-feature.test.cpp" -> "my-feature.test"
std::string SnapshotStemForFile(const std::string& filePath);

} // namespace Cimmerian::Snapshot
