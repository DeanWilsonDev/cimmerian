#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include "test-assertions.hpp"
#include "test-macro-helpers.hpp"
#include "test-runner.hpp"
#include "test-registry.hpp"
#include "test-case.hpp"
#include "test-group.hpp"
#include "test-mode.hpp"

/* ################################# */
/* ========= FRAMEWORK STATE ======= */
/* ################################# */

// Use the registry's own singleton rather than a raw inline pointer
inline TestGroup* _test_group = nullptr;

// Combined mode of the enclosing DESCRIBE / DESCRIBE_SKIP / DESCRIBE_ONLY
// blocks; every test registered inside them inherits it.
inline Cimmerian::TestMode _test_scope_mode = Cimmerian::TestMode::Normal;

/* ################################# */
/* ========= DESCRIBE: ============= */
/* ################################# */

// Variadic so a BODY whose expansion contains top-level commas survives
// being forwarded through DESCRIBE / DESCRIBE_SKIP / DESCRIBE_ONLY.
#define CIMMERIAN_DESCRIBE_WITH_MODE(mode, group_name, ...)                                        \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_test_framework_desc_reg_, __COUNTER__) =     \
      []() {                                                                                       \
        Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();               \
        TestGroup* _parent = (_test_group != nullptr) ? _test_group : _registry.GetRootGroup();    \
        TestGroup* _child = _registry.GetChildGroup(_parent, (group_name));                        \
        TestGroup* _saved = _test_group;                                                           \
        const Cimmerian::TestMode _savedMode = _test_scope_mode;                                   \
        _test_group = _child;                                                                      \
        _test_scope_mode = Cimmerian::CombineTestModes(_savedMode, (mode));                        \
        do                                                                                         \
          __VA_ARGS__ while (0);                                                                   \
        _test_group = _saved;                                                                      \
        _test_scope_mode = _savedMode;                                                             \
        return true;                                                                               \
      }();

#define DESCRIBE(group_name, BODY)                                                                 \
  CIMMERIAN_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Normal, group_name, BODY)
#define DESCRIBE_SKIP(group_name, BODY)                                                            \
  CIMMERIAN_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Skip, group_name, BODY)
#define DESCRIBE_ONLY(group_name, BODY)                                                            \
  CIMMERIAN_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Only, group_name, BODY)

/* ################################# */
/* ========= HOOKS: ================ */
/* ################################# */

#define BEFORE_EACH(BODY)                                                                          \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetBeforeEach(                                          \
        _test_group, +[]([[maybe_unused]] void* user) BODY, nullptr, nullptr                                        \
    );                                                                                             \
  } while (0)

#define AFTER_EACH(BODY)                                                                           \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetAfterEach(                                           \
        _test_group, +[]([[maybe_unused]] void* user) BODY, nullptr, nullptr                                        \
    );                                                                                             \
  } while (0)

#define BEFORE_ALL(BODY)                                                                           \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetBeforeAll(                                           \
        _test_group, +[]([[maybe_unused]] void* user) BODY, nullptr, nullptr                                        \
    );                                                                                             \
  } while (0)

#define AFTER_ALL(BODY)                                                                            \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetAfterAll(                                            \
        _test_group, +[]([[maybe_unused]] void* user) BODY, nullptr, nullptr                                        \
    );                                                                                             \
  } while (0)

/* ################################# */
/* =========== TESTS: ============== */
/* ################################# */

#define CIMMERIAN_TEST_WITH_MODE(mode, testName, ...)                                              \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(                                              \
      _test_framework_test_body_reg_, __COUNTER__                                                  \
  ) = [=]() {                                                                                      \
    Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();                   \
    TestGroup* _group = _test_group ? _test_group : _registry.GetRootGroup();                      \
    _registry.RegisterTest(                                                                        \
        _group, (testName),                                                                        \
        +[](void* user) {                                                                          \
          (void)user;                                                                              \
          __VA_ARGS__                                                                              \
        },                                                                                         \
        nullptr, nullptr, Cimmerian::CombineTestModes(_test_scope_mode, (mode))                    \
    );                                                                                             \
    return true;                                                                                   \
  }();

#define CIMMERIAN_TEST_FN_WITH_MODE(mode, testName, FN)                                            \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_test_framework_test_fn_reg_, __COUNTER__) =  \
      [=]() {                                                                                      \
        Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();               \
        TestGroup* _group = _test_group ? _test_group : _registry.GetRootGroup();                  \
        _registry.RegisterTest(                                                                    \
            _group, (testName), (FN), nullptr, nullptr,                                            \
            Cimmerian::CombineTestModes(_test_scope_mode, (mode))                                  \
        );                                                                                         \
        return true;                                                                               \
      }();

#define TEST(testName, ...)                                                                        \
  CIMMERIAN_TEST_WITH_MODE(Cimmerian::TestMode::Normal, testName, __VA_ARGS__)
#define TEST_SKIP(testName, ...)                                                                   \
  CIMMERIAN_TEST_WITH_MODE(Cimmerian::TestMode::Skip, testName, __VA_ARGS__)
#define TEST_ONLY(testName, ...)                                                                   \
  CIMMERIAN_TEST_WITH_MODE(Cimmerian::TestMode::Only, testName, __VA_ARGS__)

#define TEST_FN(testName, FN)                                                                      \
  CIMMERIAN_TEST_FN_WITH_MODE(Cimmerian::TestMode::Normal, testName, FN)
#define TEST_FN_SKIP(testName, FN)                                                                 \
  CIMMERIAN_TEST_FN_WITH_MODE(Cimmerian::TestMode::Skip, testName, FN)
#define TEST_FN_ONLY(testName, FN)                                                                 \
  CIMMERIAN_TEST_FN_WITH_MODE(Cimmerian::TestMode::Only, testName, FN)

#define IT(testName, BODY) TEST(testName, BODY)
#define IT_SKIP(testName, BODY) TEST_SKIP(testName, BODY)
#define IT_ONLY(testName, BODY) TEST_ONLY(testName, BODY)

#define IT_FN(testName, FN) TEST_FN(testName, FN)
#define IT_FN_SKIP(testName, FN) TEST_FN_SKIP(testName, FN)
#define IT_FN_ONLY(testName, FN) TEST_FN_ONLY(testName, FN)

/* ################################# */
/* ========= ASSERTIONS: =========== */
/* ################################# */

#define ASSERT_TRUE(cond)                                                                          \
  do {                                                                                             \
    if (!(cond))                                                                                   \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "ASSERT_TRUE failed: " #cond                                         \
      );                                                                                           \
  } while (0)

#define ASSERT_FALSE(cond)                                                                         \
  do {                                                                                             \
    if ((cond))                                                                                    \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "ASSERT_FALSE failed: " #cond                                        \
      );                                                                                           \
  } while (0)

#define ASSERT_EQUAL(a, b)                                                                         \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_equal((a), (b), __FILE__, __LINE__);                           \
  } while (0)

#define ASSERT_NEAR(a, b, epsilon)                                                                 \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_near((a), (b), (epsilon), __FILE__, __LINE__);                 \
  } while (0)

#define ASSERT_NOT_EQUAL(a, b)                                                                     \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_not_equal((a), (b), __FILE__, __LINE__);                       \
  } while (0)

#define ASSERT_NULL(ptr)                                                                           \
  do {                                                                                             \
    if ((ptr) != nullptr)                                                                          \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "ASSERT_NULL failed: " #ptr " is not null"                           \
      );                                                                                           \
  } while (0)

#define ASSERT_NOT_NULL(ptr)                                                                       \
  do {                                                                                             \
    if ((ptr) == nullptr)                                                                          \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "ASSERT_NOT_NULL failed: " #ptr " is null"                           \
      );                                                                                           \
  } while (0)

// REQUIRE variants halt the current test on failure via return
#define REQUIRE_TRUE(cond)                                                                         \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "REQUIRE_TRUE failed: " #cond                                        \
      );                                                                                           \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#define REQUIRE_EQUAL(a, b)                                                                        \
  do {                                                                                             \
    if (!((a) == (b))) {                                                                           \
      ::Cimmerian::Assertions::assert_equal((a), (b), __FILE__, __LINE__);                         \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#define ASSERT_THROWS(expression, exception_type)                                                  \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_throws<exception_type>(                                        \
        [&]() { (expression); }, #expression, __FILE__, __LINE__                                   \
    );                                                                                             \
  } while (0)


#define CAPTURE_FAILURE(expression)                                                                \
  Cimmerian::TestRunner::GetActive()->CaptureFailure([&]() { expression; })

#define CAPTURE_FAILURE_MESSAGE(expression)                                                        \
  Cimmerian::TestRunner::GetActive()->CaptureFailureMessage([&]() { expression; })

#define ASSERT_FAILS(expression)                                                                   \
  do {                                                                                             \
    const bool _assertFailsResult =                                                                \
        Cimmerian::TestRunner::GetActive()->ExpectFailure([&]() { expression; });                  \
    if (!_assertFailsResult) {                                                                     \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__,                                                                      \
          "ASSERT_FAILS: expected a failure but none occurred: " #expression                       \
      );                                                                                           \
    }                                                                                              \
  } while (0)

#define ASSERT_NO_THROW(expression)                                                                \
  do {                                                                                             \
    try {                                                                                          \
      (expression);                                                                                \
    }                                                                                              \
    catch (const std::exception& _e) {                                                             \
      const std::string _assertNoThrowMessage =                                                    \
          "ASSERT_NO_THROW failed: " #expression " threw: " + std::string(_e.what());              \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, _assertNoThrowMessage.c_str()                                        \
      );                                                                                           \
    }                                                                                              \
    catch (...) {                                                                                  \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                            \
          __FILE__, __LINE__, "ASSERT_NO_THROW failed: " #expression " threw unknown exception"    \
      );                                                                                           \
    }                                                                                              \
  } while (0)
