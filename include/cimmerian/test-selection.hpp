#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TestCase;
class TestGroup;

namespace Cimmerian {

// --shard <index>/<count>: run the index-th of count slices, counting from 1.
struct ShardSpec {
  int index = 1;
  int count = 1;
};

// Parses "<index>/<count>" with 1 <= index <= count. Anything else, such as
// "0/4", "5/4" or "2/4x", gives std::nullopt.
std::optional<ShardSpec> ParseShardSpec(std::string_view text);

// "Group > Subgroup > test name", or just the test name for a test outside
// every DESCRIBE. --filter and --list use this form.
std::string JoinTestName(std::string_view groupPath, std::string_view testName);

// Whether pattern is a case-sensitive substring of fullName. An empty pattern
// matches every test.
bool MatchesTestFilter(std::string_view fullName, std::string_view pattern);

// Spreads units of work across shardCount shards: largest first, each to the
// shard with the fewest tests so far. Ties go to the earlier unit and the
// lower shard, so every process given the same counts gets the same answer.
// Returns each unit's shard, counting from 1.
std::vector<int> AssignShards(const std::vector<int>& unitTestCounts, int shardCount);

// The flags that pick which tests a run covers.
struct TestSelectionOptions {
  std::string filter;
  bool listOnly = false;
  std::optional<ShardSpec> shard;
  std::string error; // why the command line can't be used, or empty
};

// Reads --filter <pattern>, --list and --shard <index>/<count> from argv,
// ignoring every other argument. A later flag replaces an earlier one. A
// missing or bad value is reported in the result's error.
TestSelectionOptions ParseTestSelectionArgs(int argc, const char* const argv[]);

// Process-wide, like TestModeRegistry, so the unit and visual runners in one
// binary pick tests with the same flags.
class TestSelectionRegistry {
public:
  static TestSelectionRegistry& GetInstance()
  {
    static TestSelectionRegistry singletonInstance;
    return singletonInstance;
  }

  void ParseArgs(int argc, char* argv[]) { this->options = ParseTestSelectionArgs(argc, argv); }

  const TestSelectionOptions& Get() const { return this->options; }
  void Set(TestSelectionOptions newOptions) { this->options = std::move(newOptions); }

  TestSelectionRegistry(const TestSelectionRegistry&) = delete;
  TestSelectionRegistry& operator=(const TestSelectionRegistry&) = delete;

private:
  TestSelectionRegistry() = default;
  TestSelectionOptions options;
};

struct SelectedTest {
  const TestGroup* group;
  const TestCase* test;
  std::string fullName;
};

struct TestSelection {
  std::vector<SelectedTest> tests; // the tests this process covers, in run order
  int registeredCount = 0;         // every test in the tree
  int matchedCount = 0;            // tests the filter matches, across every shard
};

// The tests this process covers: those whose full name matches filter and,
// under a shard, whose top-level DESCRIBE falls in this shard. A top-level
// DESCRIBE goes to one shard with everything nested in it, and the tests
// outside every DESCRIBE form one more unit. _SKIP'd tests are included, so
// the runner can still print them. shouldRun says whether a test would
// execute, and only weighs the units when sharding.
TestSelection SelectTests(
    const TestGroup* root,
    std::string_view filter,
    const std::optional<ShardSpec>& shard,
    const std::function<bool(const TestCase&)>& shouldRun
);

} // namespace Cimmerian
