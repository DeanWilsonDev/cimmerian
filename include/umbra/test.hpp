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

/* ################################# */
/* ========= FRAMEWORK STATE ======= */
/* ################################# */

// Use the registry's own singleton rather than a raw inline pointer
inline TestGroup* _test_group = nullptr;

/* ################################# */
/* ========= DESCRIBE: ============= */
/* ################################# */

#define DESCRIBE(group_name, BODY)                                                                 \
  UMBRA_MAYBE_UNUSED static const bool MACRO_CAT(_test_framework_desc_reg_, __COUNTER__) = []() {  \
    Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();                                         \
    TestGroup* _parent = (_test_group != nullptr) ? _test_group : _registry.GetRootGroup();        \
    TestGroup* _child = _registry.GetChildGroup(_parent, (group_name));                            \
    TestGroup* _saved = _test_group;                                                               \
    _test_group = _child;                                                                          \
    do                                                                                             \
      BODY while (0);                                                                              \
    _test_group = _saved;                                                                          \
    return true;                                                                                   \
  }();

/* ################################# */
/* ========= HOOKS: ================ */
/* ################################# */

#define BEFORE_EACH(BODY)                                                                          \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetBeforeEach(                                                     \
        _test_group, +[](void* user) BODY, nullptr, nullptr                                        \
    );                                                                                             \
  } while (0)

#define AFTER_EACH(BODY)                                                                           \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetAfterEach(_test_group, +[](void* user) BODY, nullptr, nullptr); \
  } while (0)

#define BEFORE_ALL(BODY)                                                                           \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetBeforeAll(_test_group, +[](void* user) BODY, nullptr, nullptr); \
  } while (0)

#define AFTER_ALL(BODY)                                                                            \
  do {                                                                                             \
    Cimmerian::TestRegistry::GetInstance().SetAfterAll(_test_group, +[](void* user) BODY, nullptr, nullptr);  \
  } while (0)

/* ################################# */
/* =========== TESTS: ============== */
/* ################################# */

#define TEST(testName, ...)                                                                        \
  UMBRA_MAYBE_UNUSED static const bool MACRO_CAT(_test_framework_test_body_reg_, __COUNTER__) =    \
      [=]() {                                                                                      \
        Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();                                     \
        TestGroup* _group = _test_group ? _test_group : _registry.GetRootGroup();                  \
        _registry.RegisterTest(                                                                    \
            _group, (testName),                                                                    \
            +[](void* user) {                                                                      \
              (void)user;                                                                          \
              __VA_ARGS__                                                                          \
            },                                                                                     \
            nullptr, nullptr                                                                       \
        );                                                                                         \
        return true;                                                                               \
      }();

#define TEST_FN(testName, FN)                                                                      \
  UMBRA_MAYBE_UNUSED static const bool MACRO_CAT(_test_framework_test_fn_reg_, __COUNTER__) =      \
      [=]() {                                                                                      \
        Cimmerian::TestRegistry& _registry = Cimmerian::TestRegistry::GetInstance();                                     \
        TestGroup* _group = _test_group ? _test_group : _registry.GetRootGroup();                  \
        _registry.RegisterTest(_group, (testName), (FN), nullptr, nullptr);                        \
        return true;                                                                               \
      }();

#define IT(testName, BODY) TEST(testName, BODY)
#define IT_FN(testName, FN) TEST_FN(testName, FN)

/* ################################# */
/* ========= ASSERTIONS: =========== */
/* ################################# */

#define ASSERT_TRUE(cond)                                                                          \
  do {                                                                                             \
    if (!(cond))                                                                                   \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                          \
          __FILE__, __LINE__, "ASSERT_TRUE failed: " #cond                                         \
      );                                                                                           \
  } while (0)

#define ASSERT_FALSE(cond)                                                                         \
  do {                                                                                             \
    if ((cond))                                                                                    \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                          \
          __FILE__, __LINE__, "ASSERT_FALSE failed: " #cond                                        \
      );                                                                                           \
  } while (0)

#define ASSERT_EQUAL(a, b)                                                                         \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_equal((a), (b), __FILE__, __LINE__);                         \
  } while (0)

#define ASSERT_NOT_EQUAL(a, b)                                                                     \
  do {                                                                                             \
    ::Cimmerian::Assertions::assert_not_equal((a), (b), __FILE__, __LINE__);                     \
  } while (0)

// REQUIRE variants halt the current test on failure via return
#define REQUIRE_TRUE(cond)                                                                         \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      Cimmerian::TestFailHandlerRegistry::GetInstance().NotifyTestFail(                          \
          __FILE__, __LINE__, "REQUIRE_TRUE failed: " #cond                                        \
      );                                                                                           \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#define REQUIRE_EQUAL(a, b)                                                                        \
  do {                                                                                             \
    if (!((a) == (b))) {                                                                           \
      ::Cimmerian::Assertions::assert_equal((a), (b), __FILE__, __LINE__);                       \
      return;                                                                                      \
    }                                                                                              \
  } while (0)
