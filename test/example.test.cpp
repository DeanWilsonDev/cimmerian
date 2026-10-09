#include "cimmerian/snapshot.hpp"
#include "cimmerian/test.hpp"
#include <stdexcept>
#include <string>
#include <vector>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static int beforeEachCallCount = 0;
static int afterEachCallCount  = 0;
static int beforeAllCallCount  = 0;
static int afterAllCallCount   = 0;

struct Point {
  int x;
  int y;
  bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

// Capture a failure message and snapshot it, asserting one was produced.
// The message carries ANSI color codes (it's the exact text a terminal would
// show); strip those before snapshotting so the .snap file stays plain,
// human-readable text rather than a wall of escape codes.
#define ASSERT_FAILURE_SNAPSHOT(expression, label)                                                 \
  do {                                                                                             \
    auto _capturedMessage = CAPTURE_FAILURE_MESSAGE(expression);                                   \
    ASSERT_TRUE(_capturedMessage.has_value());                                                     \
    ASSERT_STRING_SNAPSHOT(Cimmerian::Ansi::AnsiFormatter::StripCodes(*_capturedMessage), label);  \
  } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_TRUE / ASSERT_FALSE
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Boolean Assertions", {
  TEST("ASSERT_TRUE passes for a true expression", {
    ASSERT_TRUE(1 == 1);
    ASSERT_TRUE(true);
  });

  TEST("ASSERT_FALSE passes for a false expression", {
    ASSERT_FALSE(1 == 2);
    ASSERT_FALSE(false);
  });

  TEST("ASSERT_TRUE produces a failure for a false expression", {
    ASSERT_FAILS(ASSERT_TRUE(1 == 2));
  });

  TEST("ASSERT_FALSE produces a failure for a true expression", {
    ASSERT_FAILS(ASSERT_FALSE(1 == 1));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_EQUAL — scalars
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Scalar Equality", {
  TEST("ASSERT_EQUAL passes for equal ints", {
    ASSERT_EQUAL(42, 42);
  });

  TEST("ASSERT_EQUAL passes for equal doubles", {
    ASSERT_EQUAL(3.14, 3.14);
  });

  TEST("ASSERT_EQUAL failure output — unequal ints", {
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(1, 2), "scalar-int-diff");
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_NOT_EQUAL — scalars
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Scalar Inequality", {
  TEST("ASSERT_NOT_EQUAL passes for different ints", {
    ASSERT_NOT_EQUAL(1, 2);
  });

  TEST("ASSERT_NOT_EQUAL produces a failure for equal ints", {
    ASSERT_FAILS(ASSERT_NOT_EQUAL(5, 5));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_NEAR
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Near Equality", {
  TEST("ASSERT_NEAR passes when difference is within epsilon", {
    ASSERT_NEAR(1.0, 1.001, 0.01);
  });

  TEST("ASSERT_NEAR failure output — difference exceeds epsilon", {
    ASSERT_FAILURE_SNAPSHOT(ASSERT_NEAR(1.0, 2.0, 0.01), "near-epsilon-exceeded");
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_EQUAL — strings
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("String Equality", {
  TEST("ASSERT_EQUAL passes for identical string literals", {
    ASSERT_EQUAL("hello", "hello");
  });

  TEST("ASSERT_EQUAL passes for identical std::strings", {
    const std::string actual   = "cimmerian";
    const std::string expected = "cimmerian";
    ASSERT_EQUAL(actual, expected);
  });

  TEST("ASSERT_EQUAL failure output — character-level diff", {
    ASSERT_FAILURE_SNAPSHOT(
      ASSERT_EQUAL(std::string("received"), std::string("expected")),
      "string-char-diff"
    );
  });

  TEST("ASSERT_EQUAL failure output — actual shorter than expected", {
    ASSERT_FAILURE_SNAPSHOT(
      ASSERT_EQUAL(std::string("hi"), std::string("hello")),
      "string-actual-shorter"
    );
  });

  TEST("ASSERT_EQUAL failure output — actual longer than expected", {
    ASSERT_FAILURE_SNAPSHOT(
      ASSERT_EQUAL(std::string("hello world"), std::string("hello")),
      "string-actual-longer"
    );
  });

  TEST("ASSERT_NOT_EQUAL passes for different strings", {
    ASSERT_NOT_EQUAL(std::string("foo"), std::string("bar"));
  });

  TEST("ASSERT_NOT_EQUAL produces a failure for identical strings", {
    ASSERT_FAILS(ASSERT_NOT_EQUAL(std::string("same"), std::string("same")));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_EQUAL — C-style arrays
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("C-Style Array Equality", {
  TEST("ASSERT_EQUAL passes for identical int arrays", {
    int actual[]   = {1, 2, 3};
    int expected[] = {1, 2, 3};
    ASSERT_EQUAL(actual, expected);
  });

  TEST("ASSERT_EQUAL failure output — differing elements", {
    int actual[]   = {1, 99, 3};
    int expected[] = {1, 2,  3};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "array-element-diff");
  });

  TEST("ASSERT_EQUAL failure output — different sizes", {
    int actual[]   = {1, 2, 3};
    int expected[] = {1, 2};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "array-size-diff");
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_EQUAL — std::vector
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Vector Equality", {
  TEST("ASSERT_EQUAL passes for identical vectors", {
    std::vector<int> actual   = {10, 20, 30};
    std::vector<int> expected = {10, 20, 30};
    ASSERT_EQUAL(actual, expected);
  });

  TEST("ASSERT_EQUAL failure output — actual has extra elements", {
    std::vector<int> actual   = {1, 2, 3};
    std::vector<int> expected = {1, 2};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "vector-extra-elements");
  });

  TEST("ASSERT_EQUAL failure output — actual missing elements", {
    std::vector<int> actual   = {1, 2};
    std::vector<int> expected = {1, 2, 3};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "vector-missing-elements");
  });

  TEST("ASSERT_EQUAL failure output — element-level mismatches", {
    std::vector<int> actual   = {1, 99, 3};
    std::vector<int> expected = {1, 2,  3};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "vector-element-diff");
  });

  TEST("ASSERT_NOT_EQUAL passes for different vectors", {
    std::vector<int> actual   = {1, 2, 3};
    std::vector<int> expected = {4, 5, 6};
    ASSERT_NOT_EQUAL(actual, expected);
  });

  TEST("ASSERT_NOT_EQUAL produces a failure for identical vectors", {
    ASSERT_FAILS(ASSERT_NOT_EQUAL((std::vector<int>{1, 2, 3}), (std::vector<int>{1, 2, 3})));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_EQUAL — std::array
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("std::array Equality", {
  TEST("ASSERT_EQUAL passes for identical std::arrays", {
    std::array<int, 3> actual   = {7, 8, 9};
    std::array<int, 3> expected = {7, 8, 9};
    ASSERT_EQUAL(actual, expected);
  });

  TEST("ASSERT_EQUAL failure output — mismatched std::arrays", {
    std::array<int, 3> actual   = {7, 0, 9};
    std::array<int, 3> expected = {7, 8, 9};
    ASSERT_FAILURE_SNAPSHOT(ASSERT_EQUAL(actual, expected), "std-array-element-diff");
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Failure records — what a handler receives
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Failure Records", {
  TEST("a diffing assertion carries its diff as detail lines, not in the message", {
    std::vector<int> actual   = {1, 2, 3};
    std::vector<int> expected = {1, 2};
    auto failure = CAPTURE_FAILURE(ASSERT_EQUAL(actual, expected));
    REQUIRE_TRUE(failure.has_value());
    ASSERT_EQUAL(failure->message, "Containers differ:");
    REQUIRE_EQUAL(failure->details.size(), std::size_t {2});
    ASSERT_EQUAL(Cimmerian::Ansi::AnsiFormatter::StripCodes(failure->details[0]), "Expected  [1, 2]");
    ASSERT_EQUAL(Cimmerian::Ansi::AnsiFormatter::StripCodes(failure->details[1]), "Received  [1, 2, 3]");
  });

  TEST("a one-line failure has no detail lines", {
    auto failure = CAPTURE_FAILURE(ASSERT_TRUE(1 == 2));
    REQUIRE_TRUE(failure.has_value());
    ASSERT_EQUAL(failure->message, "ASSERT_TRUE failed: 1 == 2");
    ASSERT_TRUE(failure->details.empty());
  });

  TEST("a preformatted multi-line message splits into a message and detail lines", {
    auto failure = CAPTURE_FAILURE(
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(
        __FILE__, __LINE__, "lint is not clean:\n  - first\n\n  - second\n"
      )
    );
    REQUIRE_TRUE(failure.has_value());
    ASSERT_EQUAL(failure->message, "lint is not clean:");
    const std::vector<std::string> expectedDetails = {"  - first", "", "  - second"};
    ASSERT_EQUAL(failure->details, expectedDetails);
  });

  TEST("CAPTURE_FAILURE_MESSAGE joins the message and detail lines", {
    std::vector<int> actual   = {1, 2, 3};
    std::vector<int> expected = {1, 2};
    auto message = CAPTURE_FAILURE_MESSAGE(ASSERT_EQUAL(actual, expected));
    REQUIRE_TRUE(message.has_value());
    ASSERT_EQUAL(
      Cimmerian::Ansi::AnsiFormatter::StripCodes(*message),
      "Containers differ:\nExpected  [1, 2]\nReceived  [1, 2, 3]"
    );
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_NULL / ASSERT_NOT_NULL
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Null Assertions", {
  TEST("ASSERT_NULL passes for a null pointer", {
    int* ptr = nullptr;
    ASSERT_NULL(ptr);
  });

  TEST("ASSERT_NOT_NULL passes for a non-null pointer", {
    int value = 42;
    int* ptr  = &value;
    ASSERT_NOT_NULL(ptr);
  });

  TEST("ASSERT_NULL produces a failure for a non-null pointer", {
    int value = 1;
    int* ptr  = &value;
    ASSERT_FAILS(ASSERT_NULL(ptr));
  });

  TEST("ASSERT_NOT_NULL produces a failure for a null pointer", {
    int* ptr = nullptr;
    ASSERT_FAILS(ASSERT_NOT_NULL(ptr));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_THROWS / ASSERT_NO_THROW
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Exception Assertions", {
  TEST("ASSERT_THROWS passes when the expected exception is thrown", {
    ASSERT_THROWS(throw std::runtime_error("boom"), std::runtime_error);
  });

  TEST("ASSERT_NO_THROW passes when no exception is thrown", {
    ASSERT_NO_THROW((void)(1 + 1));
  });

  TEST("ASSERT_THROWS produces a failure when no exception is thrown", {
    ASSERT_FAILS(ASSERT_THROWS((void)(1 + 1), std::runtime_error));
  });

  TEST("ASSERT_THROWS produces a failure when the wrong exception type is thrown", {
    ASSERT_FAILS(ASSERT_THROWS(throw std::logic_error("wrong kind"), std::runtime_error));
  });

  TEST("ASSERT_NO_THROW produces a failure when an exception is thrown", {
    ASSERT_FAILS(ASSERT_NO_THROW(throw std::runtime_error("unexpected")));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Contained test stages — an escaping exception fails the test, not the run
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Contained Test Stages", {
  TEST("a stage that completes reports nothing", {
    bool completed = false;
    auto failure = CAPTURE_FAILURE(
      completed = Cimmerian::TestRunner::GetActive()->RunContained("The test", [] {})
    );
    ASSERT_TRUE(completed);
    ASSERT_FALSE(failure.has_value());
  });

  TEST("a std::exception escaping a stage becomes a failure carrying what()", {
    bool completed = true;
    auto failure = CAPTURE_FAILURE(
      completed = Cimmerian::TestRunner::GetActive()->RunContained(
        "The test", [] { throw std::runtime_error("first line\nsecond line"); }
      )
    );
    ASSERT_FALSE(completed);
    REQUIRE_TRUE(failure.has_value());
    ASSERT_EQUAL(failure->message, "The test threw an exception:");
    const std::vector<std::string> expectedDetails = {"first line", "second line"};
    ASSERT_EQUAL(failure->details, expectedDetails);
    ASSERT_TRUE(failure->file.empty());
  });

  TEST("anything else escaping a stage becomes a failure too", {
    bool completed = true;
    auto failure = CAPTURE_FAILURE(
      completed = Cimmerian::TestRunner::GetActive()->RunContained("BEFORE_EACH", [] { throw 42; })
    );
    ASSERT_FALSE(completed);
    REQUIRE_TRUE(failure.has_value());
    ASSERT_EQUAL(failure->message, "BEFORE_EACH threw something that isn't a std::exception");
    ASSERT_TRUE(failure->details.empty());
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// REQUIRE_TRUE / REQUIRE_EQUAL — halt-on-failure variants
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Require Variants", {
  TEST("REQUIRE_TRUE passes and execution continues", {
    REQUIRE_TRUE(1 == 1);
    ASSERT_TRUE(true);
  });

  TEST("REQUIRE_EQUAL passes and execution continues", {
    REQUIRE_EQUAL(7, 7);
    ASSERT_TRUE(true);
  });

  TEST("REQUIRE_TRUE produces a failure and halts the test body", {
    ASSERT_FAILS(REQUIRE_TRUE(false));
  });

  TEST("REQUIRE_EQUAL produces a failure and halts the test body", {
    ASSERT_FAILS(REQUIRE_EQUAL(1, 99));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// IT alias
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("IT Alias", {
  IT("works identically to TEST", {
    ASSERT_EQUAL(2 + 2, 4);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// TEST_FN / IT_FN — function-pointer registration
// ─────────────────────────────────────────────────────────────────────────────

static void ExternalTestFunction(void*)
{
  ASSERT_EQUAL(6, 2 * 3);
}

DESCRIBE("Function Pointer Registration", {
  TEST_FN("TEST_FN registers and runs an external function", ExternalTestFunction);
  IT_FN("IT_FN registers and runs an external function",     ExternalTestFunction);
});

// ─────────────────────────────────────────────────────────────────────────────
// Commas in macro bodies — braces don't shield commas from the preprocessor,
// so every macro that takes a body takes it variadically
// ─────────────────────────────────────────────────────────────────────────────

struct Color {
  int r;
  int g;
  int b;
  int a;
};

static Color hookColor     = {0, 0, 0, 0};
static Color allHooksColor = {0, 0, 0, 0};

DESCRIBE("Commas In Macro Bodies", {
  static constexpr Color describeColor{1, 2, 3, 4};

  BEFORE_ALL({
    allHooksColor = Color{9, 9, 9, 9};
  });

  AFTER_ALL({
    allHooksColor = Color{0, 0, 0, 0};
  });

  BEFORE_EACH({
    hookColor = Color{5, 6, 7, 8};
  });

  AFTER_EACH({
    hookColor = Color{0, 0, 0, 0};
  });

  IT("IT takes a body with a brace-initialiser", {
    Color fallback{1, 2, 3, 4};
    ASSERT_EQUAL(fallback.a, 4);
  });

  IT("the hooks take bodies with brace-initialisers", {
    ASSERT_EQUAL(hookColor.r, 5);
    ASSERT_EQUAL(allHooksColor.r, 9);
  });

  IT("DESCRIBE takes a body with a brace-initialiser", {
    ASSERT_EQUAL(describeColor.g, 2);
  });

  TEST_FN("TEST_FN takes a lambda with several captures", [expected = 3, actual = 3](void*) {
    ASSERT_EQUAL(actual, expected);
  });

  IT_FN("IT_FN takes a lambda with several captures", [expected = 3, actual = 3](void*) {
    ASSERT_EQUAL(actual, expected);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle hooks — BEFORE_ALL / AFTER_ALL / BEFORE_EACH / AFTER_EACH
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Lifecycle Hooks", {
  BEFORE_ALL({
    beforeAllCallCount  = 0;
    afterAllCallCount   = 0;
    beforeEachCallCount = 0;
    afterEachCallCount  = 0;
    beforeAllCallCount++;
  });

  AFTER_ALL({
    afterAllCallCount++;
    ASSERT_EQUAL(beforeAllCallCount, 1);
    ASSERT_EQUAL(afterAllCallCount,  1);
  });

  BEFORE_EACH({
    beforeEachCallCount++;
  });

  AFTER_EACH({
    afterEachCallCount++;
  });

  TEST("First test — BEFORE_EACH has fired once", {
    ASSERT_EQUAL(beforeEachCallCount, 1);
  });

  TEST("Second test — BEFORE_EACH has fired twice", {
    ASSERT_EQUAL(beforeEachCallCount, 2);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Nested DESCRIBE groups
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Outer Group", {
  DESCRIBE("Inner Group", {
    DESCRIBE("Deeply Nested Group", {
      TEST("assertion runs in a deeply nested group", {
        ASSERT_EQUAL(1 + 1, 2);
      });
    });

    TEST("assertion runs in the inner group", {
      ASSERT_TRUE(true);
    });
  });

  TEST("assertion runs in the outer group", {
    ASSERT_TRUE(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Custom comparable type — exercises the scalar (non-iterable) diff path
// ─────────────────────────────────────────────────────────────────────────────

DESCRIBE("Custom Type Equality", {
  TEST("ASSERT_EQUAL passes for equal custom structs", {
    ASSERT_EQUAL((Point{3, 4}), (Point{3, 4}));
  });

  TEST("ASSERT_NOT_EQUAL passes for different custom structs", {
    ASSERT_NOT_EQUAL((Point{1, 2}), (Point{3, 4}));
  });

  TEST("ASSERT_EQUAL produces a failure for unequal custom structs", {
    ASSERT_FAILS(ASSERT_EQUAL((Point{1, 2}), (Point{3, 4})));
  });

  TEST("ASSERT_FAILS itself produces a failure when no failure occurs", {
    ASSERT_FAILS(ASSERT_FAILS(ASSERT_TRUE(true)));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// _SKIP variants — skipped tests and groups never run, nor do their hooks
// ─────────────────────────────────────────────────────────────────────────────
// _ONLY can't be exercised here end to end: one _ONLY anywhere in this binary
// would focus the whole self-test run. Mode resolution is covered directly
// below instead.

static int skipGroupBeforeEachCallCount = 0;
static int skippedGroupBeforeAllCallCount = 0;
static int skippedGroupBeforeEachCallCount = 0;

static void FailingTestFunction(void*)
{
  ASSERT_TRUE(false);
}

DESCRIBE("Skipping", {
  DESCRIBE("Skipped Tests", {
    BEFORE_EACH({
      skipGroupBeforeEachCallCount++;
    });

    TEST_SKIP("TEST_SKIP never runs its body", {
      ASSERT_TRUE(false);
    });

    IT_SKIP("IT_SKIP never runs its body", {
      ASSERT_TRUE(false);
    });

    TEST_FN_SKIP("TEST_FN_SKIP never runs its function", FailingTestFunction);
    IT_FN_SKIP("IT_FN_SKIP never runs its function", FailingTestFunction);

    TEST("BEFORE_EACH only fires for tests that run", {
      ASSERT_EQUAL(skipGroupBeforeEachCallCount, 1);
    });
  });

  DESCRIBE_SKIP("Skipped Group", {
    BEFORE_ALL({
      skippedGroupBeforeAllCallCount++;
    });

    BEFORE_EACH({
      skippedGroupBeforeEachCallCount++;
    });

    TEST("tests inside DESCRIBE_SKIP never run", {
      ASSERT_TRUE(false);
    });

    DESCRIBE("Nested Group", {
      TEST("nested groups inherit the skip", {
        ASSERT_TRUE(false);
      });
    });

    DESCRIBE_ONLY("Focused Nested Group", {
      TEST_ONLY("skip wins over a nested _ONLY", {
        ASSERT_TRUE(false);
      });
    });
  });

  DESCRIBE("After Skipped Group", {
    TEST("a skipped group's hooks never fire", {
      ASSERT_EQUAL(skippedGroupBeforeAllCallCount, 0);
      ASSERT_EQUAL(skippedGroupBeforeEachCallCount, 0);
    });

    TEST("an _ONLY inside a skipped group doesn't focus the run", {
      ASSERT_FALSE(Cimmerian::TestModeRegistry::GetInstance().HasFocusedTests());
    });
  });
});

DESCRIBE("Test Mode Resolution", {
  using Cimmerian::CombineTestModes;
  using Cimmerian::IsForbiddenMode;
  using Cimmerian::ShouldRunTest;
  using Cimmerian::TestMode;

  TEST("a test inherits its enclosing DESCRIBE's mode", {
    ASSERT_TRUE(CombineTestModes(TestMode::Skip, TestMode::Normal) == TestMode::Skip);
    ASSERT_TRUE(CombineTestModes(TestMode::Only, TestMode::Normal) == TestMode::Only);
    ASSERT_TRUE(CombineTestModes(TestMode::Normal, TestMode::Normal) == TestMode::Normal);
  });

  TEST("Skip wins over Only at either level", {
    ASSERT_TRUE(CombineTestModes(TestMode::Skip, TestMode::Only) == TestMode::Skip);
    ASSERT_TRUE(CombineTestModes(TestMode::Only, TestMode::Skip) == TestMode::Skip);
  });

  TEST("without focus, everything but Skip runs", {
    ASSERT_TRUE(ShouldRunTest(TestMode::Normal, false));
    ASSERT_TRUE(ShouldRunTest(TestMode::Only, false));
    ASSERT_FALSE(ShouldRunTest(TestMode::Skip, false));
  });

  TEST("with focus, only Only runs", {
    ASSERT_TRUE(ShouldRunTest(TestMode::Only, true));
    ASSERT_FALSE(ShouldRunTest(TestMode::Normal, true));
    ASSERT_FALSE(ShouldRunTest(TestMode::Skip, true));
  });

  TEST("--forbid-only forbids Only and nothing else", {
    ASSERT_TRUE(IsForbiddenMode(TestMode::Only, true, false));
    ASSERT_FALSE(IsForbiddenMode(TestMode::Skip, true, false));
    ASSERT_FALSE(IsForbiddenMode(TestMode::Normal, true, false));
  });

  TEST("--forbid-skip forbids Skip and nothing else", {
    ASSERT_TRUE(IsForbiddenMode(TestMode::Skip, false, true));
    ASSERT_FALSE(IsForbiddenMode(TestMode::Only, false, true));
    ASSERT_FALSE(IsForbiddenMode(TestMode::Normal, false, true));
  });

  TEST("nothing is forbidden without either flag", {
    ASSERT_FALSE(IsForbiddenMode(TestMode::Only, false, false));
    ASSERT_FALSE(IsForbiddenMode(TestMode::Skip, false, false));
  });
});
