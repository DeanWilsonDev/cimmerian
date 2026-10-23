#include "cimmerian/test-timings.hpp"
#include "cimmerian/test.hpp"
#include "support/scratch-dir.hpp"
#include "support/scratch-run.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

using Cimmerian::GroupTiming;
using Cimmerian::TestMode;
using Cimmerian::TestRegistry;

using ScratchDir = CimmerianTestSupport::ScratchDir;

void WriteFile(const fs::path& path, const std::string& contents)
{
  std::ofstream out(path, std::ios::binary);
  out << contents;
}

std::vector<GroupTiming> Parse(std::string_view text)
{
  std::string error;
  std::vector<GroupTiming> timings = Cimmerian::ParseTimings(text, &error);
  return error.empty() ? timings : std::vector<GroupTiming>{{"parse error: " + error, 0, 0}};
}

bool SameTimings(const std::vector<GroupTiming>& left, const std::vector<GroupTiming>& right)
{
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (left[i].groupName != right[i].groupName || left[i].testCount != right[i].testCount ||
        left[i].microseconds != right[i].microseconds) {
      return false;
    }
  }
  return true;
}

} // namespace

DESCRIBE("Group timings", {
  DESCRIBE("Timings files", {
    TEST("writes one line per group under a comment", {
      const std::string text = Cimmerian::FormatTimings({{"Parser", 6, 1500}, {"", 1, 20}});
      ASSERT_EQUAL(
          text, std::string(
                    "# Cimmerian group timings: <microseconds>\\t<tests>\\t<top-level group>\n"
                    "1500\t6\tParser\n"
                    "20\t1\t\n"
                )
      );
    });

    TEST("reads back what it writes, including the group outside every DESCRIBE", {
      const std::vector<GroupTiming> timings =
          {{"Parser", 6, 1500}, {"", 1, 20}, {"CairnApplication -- rendering", 3, 1900000}};
      ASSERT_TRUE(SameTimings(Parse(Cimmerian::FormatTimings(timings)), timings));
    });

    TEST("leaves out a group whose name has a line break", {
      const std::string text = Cimmerian::FormatTimings({{"two\nlines", 1, 5}, {"Lexer", 1, 7}});
      ASSERT_TRUE(SameTimings(Parse(text), {{"Lexer", 1, 7}}));
    });

    TEST("skips blank lines and comments, and reads Windows line endings", {
      ASSERT_TRUE(SameTimings(
          Parse("# a comment\r\n\r\n10\t2\tParser\r\n\n# another\n5\t1\tLexer"),
          {{"Parser", 2, 10}, {"Lexer", 1, 5}}
      ));
    });

    TEST("keeps a group's last line, so concatenated files read as one", {
      const std::string first = Cimmerian::FormatTimings({{"Parser", 6, 1500}, {"Lexer", 3, 90}});
      const std::string second = Cimmerian::FormatTimings({{"Parser", 7, 1800}, {"Writer", 2, 40}});
      ASSERT_TRUE(SameTimings(
          Parse(first + second), {{"Parser", 7, 1800}, {"Lexer", 3, 90}, {"Writer", 2, 40}}
      ));
    });

    TEST("reports the first malformed line", {
      std::string error;
      Cimmerian::ParseTimings("10\t2\tParser\n10 2 Lexer\n", &error);
      ASSERT_EQUAL(
          error, std::string("line 2 isn't \"<microseconds>\\t<tests>\\t<group>\": \"10 2 Lexer\"")
      );
    });

    TEST("rejects a negative or non-numeric time or count", {
      for (const char* line : {"-5\t1\tParser", "5\t-1\tParser", "5ms\t1\tParser", "\t1\tParser"}) {
        std::string error;
        Cimmerian::ParseTimings(line, &error);
        ASSERT_FALSE(error.empty());
      }
    });

    TEST("saves a file that loads back the same", {
      const ScratchDir dir;
      const fs::path path = dir.path / "nested" / "timings.txt";
      const std::vector<GroupTiming> timings = {{"Parser", 6, 1500}, {"", 1, 20}};
      std::string error;
      REQUIRE_TRUE(Cimmerian::SaveTimings(path, timings, &error));

      const Cimmerian::LoadedTimings loaded = Cimmerian::LoadTimings(path);
      ASSERT_TRUE(loaded.found);
      ASSERT_EQUAL(loaded.error, std::string(""));
      ASSERT_TRUE(SameTimings(loaded.timings, timings));
    });

    TEST("loading a file that doesn't exist isn't an error", {
      const ScratchDir dir;
      const Cimmerian::LoadedTimings loaded = Cimmerian::LoadTimings(dir.path / "missing.txt");
      ASSERT_FALSE(loaded.found);
      ASSERT_EQUAL(loaded.error, std::string(""));
      ASSERT_TRUE(loaded.timings.empty());
    });

    TEST("loading a malformed file reports which file and line", {
      const ScratchDir dir;
      const fs::path path = dir.path / "timings.txt";
      WriteFile(path, "10\t2\tParser\nnot a timing\n");
      const Cimmerian::LoadedTimings loaded = Cimmerian::LoadTimings(path);
      ASSERT_TRUE(loaded.found);
      ASSERT_TRUE(loaded.error.starts_with(path.string() + ": line 2 isn't"));
    });
  });

  DESCRIBE("What a run records", {
    TEST("times each top-level group whole, hooks and nested groups included", {
      //   (outside every DESCRIBE)  1 test
      //   Slow                      BEFORE_ALL sleeps 20ms; 1 test, 1 _SKIP'd
      //   Slow > Nested             1 test
      //   Fast                      2 tests
      //   Skipped                   1 _SKIP'd test only
      TestRegistry registry;
      TestGroup* root = registry.GetRootGroup();
      const auto pass = [](void*) {};
      registry.RegisterTest(root, "loose", pass);
      TestGroup* slow = registry.GetChildGroup(root, "Slow");
      registry.SetBeforeAll(slow, [](void*) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      });
      registry.RegisterTest(slow, "one", pass);
      registry.RegisterTest(slow, "skipped", pass, nullptr, nullptr, TestMode::Skip);
      registry.RegisterTest(registry.GetChildGroup(slow, "Nested"), "two", pass);
      TestGroup* fast = registry.GetChildGroup(root, "Fast");
      registry.RegisterTest(fast, "one", pass);
      registry.RegisterTest(fast, "two", pass);
      registry.RegisterTest(
          registry.GetChildGroup(root, "Skipped"), "never", pass, nullptr, nullptr, TestMode::Skip
      );

      const std::vector<GroupTiming> timings =
          CimmerianTestSupport::RunScratch(registry).summary.groupTimings;

      REQUIRE_EQUAL(timings.size(), std::size_t{3});
      ASSERT_EQUAL(timings[0].groupName, std::string(""));
      ASSERT_EQUAL(timings[0].testCount, 1);
      ASSERT_EQUAL(timings[1].groupName, std::string("Slow"));
      ASSERT_EQUAL(timings[1].testCount, 2);
      ASSERT_TRUE(timings[1].microseconds >= 20000);
      ASSERT_EQUAL(timings[2].groupName, std::string("Fast"));
      ASSERT_EQUAL(timings[2].testCount, 2);
      ASSERT_TRUE(timings[2].microseconds < timings[1].microseconds);
    });
  });
});
