#include "cimmerian/test-timings.hpp"
#include "cimmerian/snapshot/atomic-file-write.hpp"
#include <charconv>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace Cimmerian {

namespace fs = std::filesystem;

template <typename TInteger> static bool ParseWholeNumber(std::string_view digits, TInteger* value)
{
  const char* end = digits.data() + digits.size();
  const auto [parsedEnd, errorCode] = std::from_chars(digits.data(), end, *value);
  return !digits.empty() && errorCode == std::errc{} && parsedEnd == end && *value >= 0;
}

std::string FormatTimings(const std::vector<GroupTiming>& timings)
{
  std::string text = "# Cimmerian group timings: <microseconds>\\t<tests>\\t<top-level group>\n";
  for (const GroupTiming& timing : timings) {
    if (timing.groupName.find_first_of("\r\n") != std::string::npos) {
      continue;
    }
    text += std::format("{}\t{}\t{}\n", timing.microseconds, timing.testCount, timing.groupName);
  }
  return text;
}

std::vector<GroupTiming> ParseTimings(std::string_view text, std::string* error)
{
  std::vector<GroupTiming> timings;
  std::unordered_map<std::string, std::size_t> indexOfGroup;

  int lineNumber = 0;
  while (!text.empty()) {
    lineNumber++;
    const std::size_t lineEnd = text.find('\n');
    std::string_view line = text.substr(0, lineEnd);
    text = (lineEnd == std::string_view::npos) ? std::string_view() : text.substr(lineEnd + 1);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    if (line.empty() || line.starts_with('#')) {
      continue;
    }

    // The name is the rest of the line, so it may be empty or hold a tab.
    const std::size_t firstTab = line.find('\t');
    const std::size_t secondTab =
        (firstTab == std::string_view::npos) ? firstTab : line.find('\t', firstTab + 1);
    GroupTiming timing;
    if (secondTab == std::string_view::npos ||
        !ParseWholeNumber(line.substr(0, firstTab), &timing.microseconds) ||
        !ParseWholeNumber(line.substr(firstTab + 1, secondTab - firstTab - 1), &timing.testCount)) {
      *error = std::format(
          "line {} isn't \"<microseconds>\\t<tests>\\t<group>\": \"{}\"", lineNumber, line
      );
      return timings;
    }
    timing.groupName = std::string(line.substr(secondTab + 1));

    const auto [existing, isNew] = indexOfGroup.try_emplace(timing.groupName, timings.size());
    if (isNew) {
      timings.push_back(std::move(timing));
    }
    else {
      timings[existing->second] = std::move(timing);
    }
  }
  return timings;
}

LoadedTimings LoadTimings(const fs::path& path)
{
  LoadedTimings loaded;
  std::error_code errorCode;
  if (!fs::exists(path, errorCode)) {
    return loaded;
  }
  loaded.found = true;

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    loaded.error = std::format("can't read {}", path.string());
    return loaded;
  }
  std::ostringstream contents;
  contents << in.rdbuf();

  std::string parseError;
  loaded.timings = ParseTimings(contents.str(), &parseError);
  if (!parseError.empty()) {
    loaded.error = std::format("{}: {}", path.string(), parseError);
  }
  return loaded;
}

bool SaveTimings(const fs::path& path, const std::vector<GroupTiming>& timings, std::string* error)
{
  return Snapshot::WriteFileAtomically(path, FormatTimings(timings), error);
}

} // namespace Cimmerian
