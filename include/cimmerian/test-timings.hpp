#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Cimmerian {

// How long one top-level DESCRIBE took, hooks included, as --save-timings
// writes it and --timings reads it back to balance --shard.
struct GroupTiming {
  std::string groupName; // a top-level DESCRIBE, or "" for the tests outside every DESCRIBE
  int testCount = 0;     // the tests that ran
  long long microseconds = 0;
};

// One line per group, "<microseconds>\t<tests>\t<group name>", under a
// comment line. Lines are independent, so shards that each save their own
// groups can be merged by concatenating their files. A group whose name has a
// line break is left out.
std::string FormatTimings(const std::vector<GroupTiming>& timings);

// Reads FormatTimings' text, skipping blank lines and lines starting with #.
// A group listed twice keeps its last line, so a concatenated file can be
// read as it is. On a malformed line, sets *error and returns what it read
// before it.
std::vector<GroupTiming> ParseTimings(std::string_view text, std::string* error);

struct LoadedTimings {
  bool found = false; // whether the file exists
  std::vector<GroupTiming> timings;
  std::string error; // why the file couldn't be read or parsed, or empty
};

LoadedTimings LoadTimings(const std::filesystem::path& path);

// Writes atomically, so a shard reading the file sees the old one or the new
// one. On failure, returns false and sets *error.
bool SaveTimings(
    const std::filesystem::path& path,
    const std::vector<GroupTiming>& timings,
    std::string* error
);

} // namespace Cimmerian
