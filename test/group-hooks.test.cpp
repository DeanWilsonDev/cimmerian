#include "cimmerian/ansi-formatter.hpp"
#include "cimmerian/test.hpp"
#include <cstdio>
#include <functional>
#include <regex>
#include <stdexcept>
#include <string>

// BEFORE_ALL and AFTER_ALL failures. Each test builds a tree in its own
// registry and runs it on a runner of its own, which hands failures back to
// this run's runner when it's destroyed. Its report goes to a temp file, so
// its [FAIL] lines don't land in this run's output.

namespace {

using Cimmerian::TestMode;
using Cimmerian::TestRegistry;
using Cimmerian::TestRunSummary;

struct ScratchRun {
  TestRunSummary summary;
  // The scratch runner's report, without colours, and with timings and line
  // numbers replaced by <time> and <line>.
  std::string output;
};

std::string ReadAll(std::FILE* stream)
{
  std::string text;
  std::rewind(stream);
  char buffer[4096];
  std::size_t count = 0;
  while ((count = std::fread(buffer, 1, sizeof(buffer), stream)) > 0) {
    text.append(buffer, count);
  }
  return text;
}

ScratchRun RunScratch(TestRegistry& registry)
{
  ScratchRun run;
  std::FILE* stream = std::tmpfile();
  {
    Cimmerian::TestRunner runner;
    if (stream) {
      runner.SetOutput(stream);
    }
    runner.RunGroup(registry.GetRootGroup(), &run.summary);
  }
  if (stream) {
    run.output = Cimmerian::Ansi::AnsiFormatter::StripCodes(ReadAll(stream));
    std::fclose(stream);
  }
  run.output = std::regex_replace(run.output, std::regex(R"(\d+\.\d+ms)"), "<time>");
  run.output = std::regex_replace(run.output, std::regex(R"((\.cpp):\d+)"), "$1:<line>");
  return run;
}

// How often each stage of a scratch tree ran.
struct StageCounts {
  int beforeAll = 0;
  int afterAll = 0;
  int beforeEach = 0;
  int afterEach = 0;
  int bodies = 0;
  int nestedBeforeAll = 0;
  int nestedAfterAll = 0;
};

//   Database                BEFORE_ALL runs setUp, then 2 tests
//   Database > Nested       1 test, 1 _SKIP'd
//   Cache                   1 test
void BuildDatabaseTree(TestRegistry& registry, StageCounts& counts, std::function<void()> setUp)
{
  TestGroup* root = registry.GetRootGroup();
  TestGroup* database = registry.GetChildGroup(root, "Database");
  registry.SetBeforeAll(database, [&counts, setUp](void*) {
    counts.beforeAll++;
    setUp();
  });
  registry.SetAfterAll(database, [&counts](void*) { counts.afterAll++; });
  registry.SetBeforeEach(database, [&counts](void*) { counts.beforeEach++; });
  registry.SetAfterEach(database, [&counts](void*) { counts.afterEach++; });
  registry.RegisterTest(database, "inserts a record", [&counts](void*) { counts.bodies++; });
  registry.RegisterTest(database, "finds a record", [&counts](void*) { counts.bodies++; });

  TestGroup* nested = registry.GetChildGroup(database, "Nested");
  registry.SetBeforeAll(nested, [&counts](void*) { counts.nestedBeforeAll++; });
  registry.SetAfterAll(nested, [&counts](void*) { counts.nestedAfterAll++; });
  registry.RegisterTest(nested, "reads", [&counts](void*) { counts.bodies++; });
  registry.RegisterTest(
      nested, "skipped one", [&counts](void*) { counts.bodies++; }, nullptr, nullptr, TestMode::Skip
  );

  TestGroup* cache = registry.GetChildGroup(root, "Cache");
  registry.RegisterTest(cache, "still runs", [&counts](void*) { counts.bodies++; });
}

//   Cache                   AFTER_ALL runs tearDown, after 1 test
//   Later                   1 passing test, 1 failing test
void BuildCacheTree(TestRegistry& registry, int& bodies, std::function<void()> tearDown)
{
  TestGroup* root = registry.GetRootGroup();
  TestGroup* cache = registry.GetChildGroup(root, "Cache");
  registry.SetAfterAll(cache, [tearDown](void*) { tearDown(); });
  registry.RegisterTest(cache, "evicts", [&bodies](void*) { bodies++; });

  TestGroup* later = registry.GetChildGroup(root, "Later");
  registry.RegisterTest(later, "passes", [&bodies](void*) { bodies++; });
  registry.RegisterTest(later, "fails", [&bodies](void*) {
    bodies++;
    ASSERT_TRUE(1 + 1 == 3);
  });
}

} // namespace

DESCRIBE("Group hook failures", {
  TEST("a failed BEFORE_ALL fails every test under its group without running them", {
    TestRegistry registry;
    StageCounts counts;
    bool connected = false;
    BuildDatabaseTree(registry, counts, [connected] { ASSERT_TRUE(connected); });

    const ScratchRun run = RunScratch(registry);

    const std::string expectedOutput = R"([Database]
[FAIL] [Database] BEFORE_ALL

      ASSERT_TRUE failed: connected

      at group-hooks.test.cpp:<line>

[FAIL] [Database] inserts a record  (not run: BEFORE_ALL failed)
[FAIL] [Database] finds a record  (not run: BEFORE_ALL failed)
[Nested]
[FAIL] [Nested] reads  (not run: BEFORE_ALL in [Database] failed)
[SKIP] [Nested] skipped one


[Cache]
[PASS] [Cache] still runs  (<time>)
[Cache] group total: <time>

)";
    ASSERT_EQUAL(run.output, expectedOutput);

    ASSERT_EQUAL(run.summary.total, 5);
    ASSERT_EQUAL(run.summary.passed, 1);
    ASSERT_EQUAL(run.summary.failed, 3);
    ASSERT_EQUAL(run.summary.skipped, 1);
    REQUIRE_EQUAL(run.summary.hookFailures.size(), std::size_t{1});
    ASSERT_EQUAL(run.summary.hookFailures[0].groupPath, std::string("Database"));
    ASSERT_EQUAL(run.summary.hookFailures[0].hookName, std::string("BEFORE_ALL"));
    ASSERT_EQUAL(run.summary.hookFailures[0].testsNotRun, 3);
  });

  TEST("a failed BEFORE_ALL skips the per-test hooks and nested groups' hooks", {
    TestRegistry registry;
    StageCounts counts;
    BuildDatabaseTree(registry, counts, [] { ASSERT_TRUE(false); });

    RunScratch(registry);

    ASSERT_EQUAL(counts.beforeAll, 1);
    ASSERT_EQUAL(counts.bodies, 1); // only Cache's test
    ASSERT_EQUAL(counts.beforeEach, 0);
    ASSERT_EQUAL(counts.afterEach, 0);
    ASSERT_EQUAL(counts.nestedBeforeAll, 0);
    ASSERT_EQUAL(counts.nestedAfterAll, 0);
  });

  TEST("AFTER_ALL still runs after a failed BEFORE_ALL, to tear down what it set up", {
    TestRegistry registry;
    StageCounts counts;
    BuildDatabaseTree(registry, counts, [] { ASSERT_TRUE(false); });

    RunScratch(registry);

    ASSERT_EQUAL(counts.afterAll, 1);
  });

  TEST("an exception escaping BEFORE_ALL fails the group's tests instead of ending the run", {
    TestRegistry registry;
    StageCounts counts;
    BuildDatabaseTree(registry, counts, [] { throw std::runtime_error("no database at :5432"); });

    const ScratchRun run = RunScratch(registry);

    const std::string expectedStart = R"([Database]
[FAIL] [Database] BEFORE_ALL

      BEFORE_ALL threw an exception:

            no database at :5432

[FAIL] [Database] inserts a record  (not run: BEFORE_ALL failed)
)";
    ASSERT_EQUAL(run.output.substr(0, expectedStart.size()), expectedStart);
    ASSERT_EQUAL(run.summary.failed, 3);
    ASSERT_EQUAL(run.summary.passed, 1);
    ASSERT_EQUAL(counts.afterAll, 1);
  });

  TEST("a group whose BEFORE_ALL passes runs as before", {
    TestRegistry registry;
    StageCounts counts;
    BuildDatabaseTree(registry, counts, [] {});

    const ScratchRun run = RunScratch(registry);

    ASSERT_EQUAL(run.summary.passed, 4);
    ASSERT_EQUAL(run.summary.failed, 0);
    ASSERT_TRUE(run.summary.hookFailures.empty());
    ASSERT_EQUAL(counts.bodies, 4);
    ASSERT_EQUAL(counts.beforeEach, 2);
    ASSERT_EQUAL(counts.nestedBeforeAll, 1);
    ASSERT_EQUAL(counts.nestedAfterAll, 1);
  });

  TEST("a failed AFTER_ALL is reported under its own group and fails the run", {
    TestRegistry registry;
    int bodies = 0;
    const int openHandles = 2;
    BuildCacheTree(registry, bodies, [openHandles] { ASSERT_EQUAL(openHandles, 0); });

    const ScratchRun run = RunScratch(registry);

    // The hook's failure stays out of Later's failing test.
    const std::string expectedOutput = R"([Cache]
[PASS] [Cache] evicts  (<time>)
[Cache] group total: <time>
[FAIL] [Cache] AFTER_ALL

      Values differ:

            Expected  0
            Received  2

      at group-hooks.test.cpp:<line>


[Later]
[PASS] [Later] passes  (<time>)
[FAIL] [Later] fails  (<time>)

      ASSERT_TRUE failed: 1 + 1 == 3

      at group-hooks.test.cpp:<line>

[Later] group total: <time>

)";
    ASSERT_EQUAL(run.output, expectedOutput);

    ASSERT_EQUAL(bodies, 3);
    ASSERT_EQUAL(run.summary.passed, 2);
    ASSERT_EQUAL(run.summary.failed, 2); // Later's failing test, and the hook
    REQUIRE_EQUAL(run.summary.hookFailures.size(), std::size_t{1});
    ASSERT_EQUAL(run.summary.hookFailures[0].groupPath, std::string("Cache"));
    ASSERT_EQUAL(run.summary.hookFailures[0].hookName, std::string("AFTER_ALL"));
    ASSERT_EQUAL(run.summary.hookFailures[0].testsNotRun, 0);
  });

  TEST("an exception escaping AFTER_ALL is reported and the run carries on", {
    TestRegistry registry;
    int bodies = 0;
    BuildCacheTree(registry, bodies, [] { throw 42; });

    const ScratchRun run = RunScratch(registry);

    ASSERT_TRUE(
        run.output.find(
            "[FAIL] [Cache] AFTER_ALL\n\n      AFTER_ALL threw something that isn't a "
            "std::exception\n"
        ) != std::string::npos
    );
    ASSERT_EQUAL(bodies, 3);
    ASSERT_EQUAL(run.summary.failed, 2);
  });

  TEST("a runner made inside a test hands failures back when it's destroyed", {
    Cimmerian::TestRunner* outerRunner = Cimmerian::TestRunner::GetActive();
    TestRegistry registry;
    StageCounts counts;
    BuildDatabaseTree(registry, counts, [] {});

    RunScratch(registry);

    ASSERT_TRUE(Cimmerian::TestRunner::GetActive() == outerRunner);
    ASSERT_FAILS(ASSERT_TRUE(false));
  });
});
