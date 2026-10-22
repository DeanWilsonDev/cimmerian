#include "cimmerian/test-selection.hpp"
#include "cimmerian/test.hpp"
#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

using Cimmerian::ShardSpec;
using Cimmerian::TestMode;
using Cimmerian::TestRegistry;

// A tree to select from, built in its own registry so the real run never sees
// it. It has no _ONLY test, since registering one would focus the real run.
//
//   (outside every DESCRIBE)  loose
//   Parser                    5 tests, one _SKIP'd
//   Parser > Nested           2 tests
//   Lexer                     3 tests
//   Writer                    2 tests
//   Empty                     no tests
std::unique_ptr<TestRegistry> MakeRegistry()
{
  auto registry = std::make_unique<TestRegistry>();
  TestGroup* root = registry->GetRootGroup();
  const auto add = [&](TestGroup* group, const char* name, TestMode mode = TestMode::Normal) {
    registry->RegisterTest(group, name, [](void*) {}, nullptr, nullptr, mode);
  };

  add(root, "loose");

  TestGroup* parser = registry->GetChildGroup(root, "Parser");
  add(parser, "handles commas");
  add(parser, "handles quotes");
  add(parser, "handles unicode", TestMode::Skip);
  add(parser, "reports errors");
  add(parser, "reports positions");
  TestGroup* nested = registry->GetChildGroup(parser, "Nested");
  add(nested, "inner one");
  add(nested, "inner two");

  TestGroup* lexer = registry->GetChildGroup(root, "Lexer");
  add(lexer, "splits words");
  add(lexer, "keeps commas");
  add(lexer, "skips spaces");

  TestGroup* writer = registry->GetChildGroup(root, "Writer");
  add(writer, "writes a line");
  add(writer, "writes nothing");

  registry->GetChildGroup(root, "Empty");
  return registry;
}

bool IsNotSkipped(const TestCase& test)
{
  return test.GetMode() != TestMode::Skip;
}

std::vector<std::string> SelectedNames(
    const TestRegistry& registry,
    std::string_view filter,
    std::optional<ShardSpec> shard = std::nullopt
)
{
  std::vector<std::string> names;
  for (const Cimmerian::SelectedTest& selected :
       Cimmerian::SelectTests(registry.GetRootGroup(), filter, shard, IsNotSkipped).tests) {
    names.push_back(selected.fullName);
  }
  return names;
}

// The top-level DESCRIBE a full name belongs to, or "" outside every DESCRIBE.
std::string TopLevelGroup(const std::string& fullName)
{
  const std::size_t separator = fullName.find(" > ");
  return separator == std::string::npos ? "" : fullName.substr(0, separator);
}

Cimmerian::TestSelectionOptions Parse(std::vector<const char*> args)
{
  args.insert(args.begin(), "test_binary");
  return Cimmerian::ParseTestSelectionArgs(static_cast<int>(args.size()), args.data());
}

} // namespace

DESCRIBE("Test Selection", {
  using Cimmerian::AssignShards;
  using Cimmerian::JoinTestName;
  using Cimmerian::MatchesTestFilter;
  using Cimmerian::ParseShardSpec;

  DESCRIBE("ParseShardSpec", {
    TEST("reads an index and a count", {
      const std::optional<ShardSpec> shard = ParseShardSpec("2/4");
      REQUIRE_TRUE(shard.has_value());
      ASSERT_EQUAL(shard->index, 2);
      ASSERT_EQUAL(shard->count, 4);
    });

    TEST("accepts a single shard", { ASSERT_TRUE(ParseShardSpec("1/1").has_value()); });

    TEST("rejects an index outside 1 to count", {
      ASSERT_FALSE(ParseShardSpec("0/4").has_value());
      ASSERT_FALSE(ParseShardSpec("5/4").has_value());
      ASSERT_FALSE(ParseShardSpec("-1/3").has_value());
    });

    TEST("rejects a count below 1", {
      ASSERT_FALSE(ParseShardSpec("1/0").has_value());
      ASSERT_FALSE(ParseShardSpec("0/0").has_value());
    });

    TEST("rejects anything that isn't two whole numbers around a slash", {
      ASSERT_FALSE(ParseShardSpec("").has_value());
      ASSERT_FALSE(ParseShardSpec("4").has_value());
      ASSERT_FALSE(ParseShardSpec("a/b").has_value());
      ASSERT_FALSE(ParseShardSpec("2/4x").has_value());
      ASSERT_FALSE(ParseShardSpec("1/2/3").has_value());
      ASSERT_FALSE(ParseShardSpec("/4").has_value());
      ASSERT_FALSE(ParseShardSpec("2/").has_value());
      ASSERT_FALSE(ParseShardSpec(" 2/4").has_value());
      ASSERT_FALSE(ParseShardSpec("+2/4").has_value());
    });
  });

  DESCRIBE("Full names and filters", {
    TEST("joins the group path and the test name", {
      ASSERT_EQUAL(
          JoinTestName("Parser > Nested", "inner one"), std::string("Parser > Nested > inner one")
      );
    });

    TEST("a test outside every DESCRIBE is just its name", {
      ASSERT_EQUAL(JoinTestName("", "loose"), std::string("loose"));
    });

    TEST("an empty filter matches every test", {
      ASSERT_TRUE(MatchesTestFilter("Parser > handles commas", ""));
    });

    TEST("a filter matches anywhere in the full name", {
      ASSERT_TRUE(MatchesTestFilter("Parser > handles commas", "Parser"));
      ASSERT_TRUE(MatchesTestFilter("Parser > handles commas", "commas"));
      ASSERT_TRUE(MatchesTestFilter("Parser > handles commas", "r > h"));
      ASSERT_FALSE(MatchesTestFilter("Parser > handles commas", "Lexer"));
    });

    TEST("a filter is case-sensitive", {
      ASSERT_FALSE(MatchesTestFilter("Parser > handles commas", "parser"));
    });
  });

  DESCRIBE("AssignShards", {
    TEST("one shard takes every unit", {
      ASSERT_EQUAL(AssignShards({5, 3, 0, 2}, 1), (std::vector<int>{1, 1, 1, 1}));
    });

    TEST("gives each unit, largest first, to the shard with the fewest tests", {
      // 5 -> shard 1; 3 -> shard 2; 3 -> shard 2 (3 < 5); 1 -> shard 1 (5 < 6).
      ASSERT_EQUAL(AssignShards({5, 3, 3, 1}, 2), (std::vector<int>{1, 2, 2, 1}));
    });

    TEST("breaks ties by the earlier unit and the lower shard", {
      ASSERT_EQUAL(AssignShards({2, 2, 2, 2}, 2), (std::vector<int>{1, 2, 1, 2}));
    });

    TEST("units with no tests to run add no load, so they all go to the lightest shard", {
      ASSERT_EQUAL(AssignShards({0, 0, 0}, 3), (std::vector<int>{1, 1, 1}));
      ASSERT_EQUAL(AssignShards({0, 4, 0}, 2), (std::vector<int>{2, 1, 2}));
    });

    TEST("leaves shards empty when there are more shards than units", {
      ASSERT_EQUAL(AssignShards({4, 2}, 4), (std::vector<int>{1, 2}));
    });

    TEST("gives every unit exactly one shard from 1 to count", {
      const std::vector<int> counts = {7, 1, 4, 4, 9, 0, 2, 2, 5, 3, 1};
      for (int shardCount = 1; shardCount <= 6; ++shardCount) {
        const std::vector<int> shards = AssignShards(counts, shardCount);
        REQUIRE_EQUAL(shards.size(), counts.size());
        for (const int shard : shards) {
          ASSERT_TRUE(shard >= 1 && shard <= shardCount);
        }
      }
    });

    TEST("gives the same answer every time", {
      const std::vector<int> counts = {7, 1, 4, 4, 9, 0, 2, 2, 5, 3, 1};
      ASSERT_EQUAL(AssignShards(counts, 4), AssignShards(counts, 4));
    });
  });

  DESCRIBE("SelectTests", {
    TEST("without a filter or shard, selects every test in run order", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      const std::vector<std::string> expected = {
          "loose",
          "Parser > handles commas",
          "Parser > handles quotes",
          "Parser > handles unicode",
          "Parser > reports errors",
          "Parser > reports positions",
          "Parser > Nested > inner one",
          "Parser > Nested > inner two",
          "Lexer > splits words",
          "Lexer > keeps commas",
          "Lexer > skips spaces",
          "Writer > writes a line",
          "Writer > writes nothing",
      };
      ASSERT_EQUAL(SelectedNames(*registry, ""), expected);
    });

    TEST("counts every registered test and every match", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      const Cimmerian::TestSelection selection = Cimmerian::SelectTests(
          registry->GetRootGroup(), "commas", ShardSpec{.index = 2, .count = 2}, IsNotSkipped
      );
      ASSERT_EQUAL(selection.registeredCount, 13);
      ASSERT_EQUAL(selection.matchedCount, 2);
    });

    TEST("a filter keeps only the tests whose full name contains it", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      ASSERT_EQUAL(
          SelectedNames(*registry, "commas"),
          (std::vector<std::string>{"Parser > handles commas", "Lexer > keeps commas"})
      );
      ASSERT_EQUAL(
          SelectedNames(*registry, "Parser > Nested"),
          (std::vector<std::string>{"Parser > Nested > inner one", "Parser > Nested > inner two"})
      );
      ASSERT_EQUAL(SelectedNames(*registry, "nothing matches this"), std::vector<std::string>{});
    });

    TEST("keeps a _SKIP'd test that matches, so the runner can print it", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      ASSERT_EQUAL(
          SelectedNames(*registry, "unicode"),
          (std::vector<std::string>{"Parser > handles unicode"})
      );
    });

    TEST("the shards together select each test exactly once", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      std::vector<std::string> everyTest = SelectedNames(*registry, "");
      std::sort(everyTest.begin(), everyTest.end());

      for (int count = 1; count <= 5; ++count) {
        std::vector<std::string> union_;
        for (int index = 1; index <= count; ++index) {
          const std::vector<std::string> shard =
              SelectedNames(*registry, "", ShardSpec{.index = index, .count = count});
          union_.insert(union_.end(), shard.begin(), shard.end());
        }
        std::sort(union_.begin(), union_.end());
        ASSERT_EQUAL(union_, everyTest);
      }
    });

    TEST("a top-level DESCRIBE and the groups nested in it share one shard", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      for (int count = 2; count <= 5; ++count) {
        std::set<std::string> groupsSeen;
        for (int index = 1; index <= count; ++index) {
          std::set<std::string> groupsInShard;
          for (const std::string& name :
               SelectedNames(*registry, "", ShardSpec{.index = index, .count = count})) {
            groupsInShard.insert(TopLevelGroup(name));
          }
          for (const std::string& group : groupsInShard) {
            ASSERT_TRUE(groupsSeen.insert(group).second);
          }
        }
      }
    });

    TEST("balances the shards by the tests that would run", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      // By tests that would run: Parser 6, Lexer 3, Writer 2, loose 1, Empty 0.
      // Parser goes to shard 1. Lexer, Writer and loose each go to shard 2,
      // which stays lighter until it also reaches 6. A shard runs its units in
      // registration order, so loose comes first.
      const std::vector<std::string> second =
          SelectedNames(*registry, "", ShardSpec{.index = 2, .count = 2});
      ASSERT_EQUAL(
          second, (std::vector<std::string>{
                      "loose",
                      "Lexer > splits words",
                      "Lexer > keeps commas",
                      "Lexer > skips spaces",
                      "Writer > writes a line",
                      "Writer > writes nothing",
                  })
      );
    });

    TEST("filters before sharding", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      // Only Parser and Lexer match, one test each, so two shards take one each.
      const ShardSpec first{.index = 1, .count = 2};
      const ShardSpec second{.index = 2, .count = 2};
      ASSERT_EQUAL(
          SelectedNames(*registry, "commas", first),
          (std::vector<std::string>{"Parser > handles commas"})
      );
      ASSERT_EQUAL(
          SelectedNames(*registry, "commas", second),
          (std::vector<std::string>{"Lexer > keeps commas"})
      );
    });

    TEST("a shard left without a unit selects nothing", {
      const std::unique_ptr<TestRegistry> registry = MakeRegistry();
      // Five units: loose, Parser, Lexer, Writer and Empty.
      ASSERT_EQUAL(
          SelectedNames(*registry, "", ShardSpec{.index = 6, .count = 6}),
          std::vector<std::string>{}
      );
    });
  });

  DESCRIBE("ParseTestSelectionArgs", {
    TEST("reads --filter, --list and --shard", {
      const Cimmerian::TestSelectionOptions options =
          Parse({"--filter", "Parser", "--list", "--shard", "2/4"});
      ASSERT_EQUAL(options.filter, std::string("Parser"));
      ASSERT_TRUE(options.listOnly);
      REQUIRE_TRUE(options.shard.has_value());
      ASSERT_EQUAL(options.shard->index, 2);
      ASSERT_EQUAL(options.shard->count, 4);
      ASSERT_EQUAL(options.error, std::string(""));
    });

    TEST("reads --flag=value too", {
      const Cimmerian::TestSelectionOptions options =
          Parse({"--filter=Parser > handles", "--shard=3/5"});
      ASSERT_EQUAL(options.filter, std::string("Parser > handles"));
      REQUIRE_TRUE(options.shard.has_value());
      ASSERT_EQUAL(options.shard->index, 3);
      ASSERT_EQUAL(options.shard->count, 5);
    });

    TEST("without any of them, selects everything", {
      const Cimmerian::TestSelectionOptions options = Parse({"--forbid-only", "--debug"});
      ASSERT_EQUAL(options.filter, std::string(""));
      ASSERT_FALSE(options.listOnly);
      ASSERT_FALSE(options.shard.has_value());
      ASSERT_EQUAL(options.error, std::string(""));
    });

    TEST("a later flag replaces an earlier one", {
      const Cimmerian::TestSelectionOptions options = Parse({"--shard", "1/2", "--shard", "2/2"});
      REQUIRE_TRUE(options.shard.has_value());
      ASSERT_EQUAL(options.shard->index, 2);
    });

    TEST("reports a bad --shard value", {
      const Cimmerian::TestSelectionOptions options = Parse({"--shard", "5/4"});
      ASSERT_FALSE(options.shard.has_value());
      ASSERT_EQUAL(
          options.error, std::string(
                             "--shard needs <index>/<count> with 1 <= index <= count, as in "
                             "--shard 2/4, but got \"5/4\""
                         )
      );
    });

    TEST("reports a --shard or --filter with nothing after it", {
      ASSERT_EQUAL(
          Parse({"--shard"}).error, std::string("--shard needs <index>/<count>, as in --shard 2/4")
      );
      ASSERT_EQUAL(
          Parse({"--filter"}).error,
          std::string("--filter needs a pattern, as in --filter \"Parser > handles commas\"")
      );
    });

    TEST("doesn't mistake a longer flag for --filter or --shard", {
      const Cimmerian::TestSelectionOptions options = Parse({"--filtered", "--sharding", "x"});
      ASSERT_EQUAL(options.filter, std::string(""));
      ASSERT_FALSE(options.shard.has_value());
      ASSERT_EQUAL(options.error, std::string(""));
    });
  });
});
