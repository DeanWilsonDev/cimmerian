#include "cimmerian/test-selection.hpp"
#include "cimmerian/test-case.hpp"
#include "cimmerian/test-group.hpp"
#include <algorithm>
#include <charconv>
#include <format>
#include <iterator>
#include <numeric>

namespace Cimmerian {

static std::optional<int> ParseWholeInt(std::string_view digits)
{
  int value = 0;
  const char* end = digits.data() + digits.size();
  const auto [parsedEnd, errorCode] = std::from_chars(digits.data(), end, value);
  if (digits.empty() || errorCode != std::errc{} || parsedEnd != end) {
    return std::nullopt;
  }
  return value;
}

std::optional<ShardSpec> ParseShardSpec(std::string_view text)
{
  const std::size_t slash = text.find('/');
  if (slash == std::string_view::npos) {
    return std::nullopt;
  }
  const std::optional<int> index = ParseWholeInt(text.substr(0, slash));
  const std::optional<int> count = ParseWholeInt(text.substr(slash + 1));
  if (!index || !count || *index < 1 || *count < 1 || *index > *count) {
    return std::nullopt;
  }
  return ShardSpec{.index = *index, .count = *count};
}

std::string JoinTestName(std::string_view groupPath, std::string_view testName)
{
  if (groupPath.empty()) {
    return std::string(testName);
  }
  return std::format("{} > {}", groupPath, testName);
}

bool MatchesTestFilter(std::string_view fullName, std::string_view pattern)
{
  return fullName.find(pattern) != std::string_view::npos;
}

std::vector<int> AssignShards(const std::vector<int>& unitTestCounts, int shardCount)
{
  std::vector<int> shardOfUnit(unitTestCounts.size(), 1);
  if (shardCount <= 1) {
    return shardOfUnit;
  }

  std::vector<std::size_t> largestFirst(unitTestCounts.size());
  std::iota(largestFirst.begin(), largestFirst.end(), 0);
  std::stable_sort(
      largestFirst.begin(), largestFirst.end(), [&](std::size_t left, std::size_t right) {
        return unitTestCounts[left] > unitTestCounts[right];
      }
  );

  std::vector<long long> shardLoad(static_cast<std::size_t>(shardCount), 0);
  for (const std::size_t unit : largestFirst) {
    // min_element returns the first of equal loads, so ties go to the lower shard.
    const auto lightest = std::min_element(shardLoad.begin(), shardLoad.end());
    *lightest += unitTestCounts[unit];
    shardOfUnit[unit] = static_cast<int>(lightest - shardLoad.begin()) + 1;
  }
  return shardOfUnit;
}

// Accepts both "--flag value" and "--flag=value". Returns the value, or
// std::nullopt if arg isn't this flag. Sets *missingValue if it is the flag
// but nothing follows it.
static std::optional<std::string_view> ReadFlagValue(
    std::string_view flag,
    int argc,
    const char* const argv[],
    int* index,
    bool* missingValue
)
{
  const std::string_view arg = argv[*index];
  if (arg.size() > flag.size() && arg.starts_with(flag) && arg[flag.size()] == '=') {
    return arg.substr(flag.size() + 1);
  }
  if (arg != flag) {
    return std::nullopt;
  }
  if (*index + 1 >= argc) {
    *missingValue = true;
    return std::nullopt;
  }
  *index += 1;
  return std::string_view(argv[*index]);
}

TestSelectionOptions ParseTestSelectionArgs(int argc, const char* const argv[])
{
  TestSelectionOptions options;
  auto reportError = [&](std::string message) {
    if (options.error.empty()) {
      options.error = std::move(message);
    }
  };

  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--list") {
      options.listOnly = true;
      continue;
    }

    bool missingValue = false;
    if (const auto filter = ReadFlagValue("--filter", argc, argv, &i, &missingValue)) {
      options.filter = std::string(*filter);
    }
    else if (missingValue) {
      reportError("--filter needs a pattern, as in --filter \"Parser > handles commas\"");
    }
    else if (const auto shardText = ReadFlagValue("--shard", argc, argv, &i, &missingValue)) {
      const std::optional<ShardSpec> shard = ParseShardSpec(*shardText);
      if (shard) {
        options.shard = shard;
      }
      else {
        reportError(
            std::format(
                "--shard needs <index>/<count> with 1 <= index <= count, as in --shard 2/4, but "
                "got \"{}\"",
                *shardText
            )
        );
      }
    }
    else if (missingValue) {
      reportError("--shard needs <index>/<count>, as in --shard 2/4");
    }
  }
  return options;
}

// Every test in group and the groups nested in it, in the order the runner
// runs them: the group's own tests, then each child group's.
static void CollectTests(const TestGroup* group, std::vector<SelectedTest>* tests)
{
  const std::string groupPath = BuildGroupPath(group);
  for (const TestCase& test : group->GetTests()) {
    tests->push_back(
        {.group = group, .test = &test, .fullName = JoinTestName(groupPath, test.GetName())}
    );
  }
  for (std::size_t i = 0; i < group->GetChildCount(); ++i) {
    CollectTests(group->GetChild(i), tests);
  }
}

TestSelection SelectTests(
    const TestGroup* root,
    std::string_view filter,
    const std::optional<ShardSpec>& shard,
    const std::function<bool(const TestCase&)>& shouldRun
)
{
  TestSelection selection;
  if (!root) {
    return selection;
  }

  // The units a shard takes whole, in run order: the tests outside every
  // DESCRIBE, then each top-level DESCRIBE with everything nested in it.
  std::vector<std::vector<SelectedTest>> units(1);
  for (const TestCase& test : root->GetTests()) {
    units[0].push_back({.group = root, .test = &test, .fullName = test.GetName()});
  }
  for (std::size_t i = 0; i < root->GetChildCount(); ++i) {
    CollectTests(root->GetChild(i), &units.emplace_back());
  }

  std::vector<int> unitWeights;
  for (std::vector<SelectedTest>& unit : units) {
    selection.registeredCount += static_cast<int>(unit.size());
    std::erase_if(unit, [&](const SelectedTest& selected) {
      return !MatchesTestFilter(selected.fullName, filter);
    });
    selection.matchedCount += static_cast<int>(unit.size());
    unitWeights.push_back(
        static_cast<int>(std::count_if(unit.begin(), unit.end(), [&](const SelectedTest& selected) {
          return shouldRun(*selected.test);
        }))
    );
  }

  const std::vector<int> shardOfUnit = AssignShards(unitWeights, shard ? shard->count : 1);
  for (std::size_t unit = 0; unit < units.size(); ++unit) {
    if (!shard || shardOfUnit[unit] == shard->index) {
      std::move(units[unit].begin(), units[unit].end(), std::back_inserter(selection.tests));
    }
  }
  return selection;
}

} // namespace Cimmerian
