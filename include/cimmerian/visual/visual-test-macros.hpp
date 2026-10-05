#pragma once

#include "../test-macro-helpers.hpp"
#include "../test-mode.hpp"
#include "navigation-driver.hpp"
#include "ui-event.hpp"
#include "visual-test-registry.hpp"
#include "visual-test-runner.hpp"

/* ################################# */
/* ========= FRAMEWORK STATE ======= */
/* ################################# */

inline Cimmerian::Visual::VisualTestGroup* _visual_test_group = nullptr;

// Combined mode of the enclosing VISUAL_DESCRIBE(_SKIP/_ONLY) blocks; every
// visual test registered inside them inherits it.
inline Cimmerian::TestMode _visual_test_scope_mode = Cimmerian::TestMode::Normal;

/* ################################# */
/* ========= VISUAL_DESCRIBE: ====== */
/* ################################# */

// Variadic so a BODY whose expansion contains top-level commas survives
// being forwarded through VISUAL_DESCRIBE / _SKIP / _ONLY.
#define CIMMERIAN_VISUAL_DESCRIBE_WITH_MODE(mode, group_name, window_handle, ...)                  \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_visual_desc_reg_, __COUNTER__) = []() {     \
    Cimmerian::Visual::VisualTestRegistry& _registry =                                             \
        Cimmerian::Visual::VisualTestRegistry::GetInstance();                                      \
    Cimmerian::Visual::VisualTestGroup* _parent =                                                  \
        (_visual_test_group != nullptr) ? _visual_test_group : _registry.GetRootGroup();           \
    Cimmerian::Visual::VisualTestGroup* _child =                                                   \
        _registry.GetChildGroup(_parent, (group_name), (window_handle));                           \
    Cimmerian::Visual::VisualTestGroup* _saved = _visual_test_group;                               \
    const Cimmerian::TestMode _savedMode = _visual_test_scope_mode;                                \
    _visual_test_group = _child;                                                                   \
    _visual_test_scope_mode = Cimmerian::CombineTestModes(_savedMode, (mode));                     \
    do                                                                                             \
      __VA_ARGS__ while (0);                                                                       \
    _visual_test_group = _saved;                                                                   \
    _visual_test_scope_mode = _savedMode;                                                          \
    return true;                                                                                   \
  }();

#define VISUAL_DESCRIBE(group_name, window_handle, BODY)                                           \
  CIMMERIAN_VISUAL_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Normal, group_name, window_handle, BODY)
#define VISUAL_DESCRIBE_SKIP(group_name, window_handle, BODY)                                      \
  CIMMERIAN_VISUAL_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Skip, group_name, window_handle, BODY)
#define VISUAL_DESCRIBE_ONLY(group_name, window_handle, BODY)                                      \
  CIMMERIAN_VISUAL_DESCRIBE_WITH_MODE(Cimmerian::TestMode::Only, group_name, window_handle, BODY)

/* ################################# */
/* ===== VISUAL_DESCRIBE_COMPONENT: */
/* ################################# */
/* A VISUAL_DESCRIBE whose group has no window handle of its own, because
   the window isn't known until each VISUAL_TEST mounts its own
   consumer-supplied component host - see docs/
   cimmerian_navigation_without_platform_input_proposal.md Proposal B.
   Pass the resulting host's window handle explicitly to ASSERT_SNAPSHOT. */

#define VISUAL_DESCRIBE_COMPONENT(group_name, BODY) VISUAL_DESCRIBE(group_name, nullptr, BODY)
#define VISUAL_DESCRIBE_COMPONENT_SKIP(group_name, BODY)                                           \
  VISUAL_DESCRIBE_SKIP(group_name, nullptr, BODY)
#define VISUAL_DESCRIBE_COMPONENT_ONLY(group_name, BODY)                                           \
  VISUAL_DESCRIBE_ONLY(group_name, nullptr, BODY)

/* ################################# */
/* =========== VISUAL_TEST: ======== */
/* ################################# */

#define CIMMERIAN_VISUAL_TEST_WITH_MODE(mode, testName, ...)                                       \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_visual_test_reg_, __COUNTER__) = [=]() {    \
    Cimmerian::Visual::VisualTestRegistry& _registry =                                             \
        Cimmerian::Visual::VisualTestRegistry::GetInstance();                                      \
    Cimmerian::Visual::VisualTestGroup* _group =                                                   \
        _visual_test_group ? _visual_test_group : _registry.GetRootGroup();                        \
    _registry.RegisterTest(                                                                        \
        _group, (testName),                                                                        \
        +[](void* user) {                                                                          \
          (void)user;                                                                              \
          __VA_ARGS__                                                                              \
        },                                                                                         \
        Cimmerian::CombineTestModes(_visual_test_scope_mode, (mode))                               \
    );                                                                                             \
    return true;                                                                                   \
  }();

#define VISUAL_TEST(testName, ...)                                                                 \
  CIMMERIAN_VISUAL_TEST_WITH_MODE(Cimmerian::TestMode::Normal, testName, __VA_ARGS__)
#define VISUAL_TEST_SKIP(testName, ...)                                                            \
  CIMMERIAN_VISUAL_TEST_WITH_MODE(Cimmerian::TestMode::Skip, testName, __VA_ARGS__)
#define VISUAL_TEST_ONLY(testName, ...)                                                            \
  CIMMERIAN_VISUAL_TEST_WITH_MODE(Cimmerian::TestMode::Only, testName, __VA_ARGS__)

/* ################################# */
/* ========= EVENTS / WAIT ========= */
/* ################################# */

#define SEND(EVENT)                                                                                \
  ::Cimmerian::Visual::VisualTestRunner::GetActive()->SendEvent(                                   \
      ::Cimmerian::Visual::UIEvent { EVENT }                                                        \
  )

#define WAIT(DURATION)                                                                             \
  ::Cimmerian::Visual::VisualTestRunner::GetActive()->SendEvent(                                   \
      ::Cimmerian::Visual::UIEvent {::Cimmerian::Visual::WaitEvent {(DURATION)}}                    \
  )

/* ################################# */
/* ========= ASSERT_SNAPSHOT ======= */
/* ################################# */

#define ASSERT_SNAPSHOT(...)                                                                       \
  ::Cimmerian::Visual::VisualTestRunner::GetActive()->AssertSnapshot(__VA_ARGS__)

/* ################################# */
/* ============ NAVIGATE =========== */
/* ################################# */
/* Jumps the app under test straight to a named screen/route/state via the
   consumer-registered NavigateFn, bypassing SEND()/IEventInjector entirely.
   See docs/cimmerian_navigation_without_platform_input_proposal.md
   Proposal A. */

#define NAVIGATE(screenKey)                                                                       \
  ::Cimmerian::Visual::ActiveNavigationDriver::GetInstance().NavigateTo((screenKey))

/* ################################# */
/* ==== EVENT CONSTRUCTOR SHIMS ==== */
/* ################################# */
/* These let SEND(MouseMove(x, y)) etc. read like free function calls while
   actually constructing the corresponding UIEvent alternative. */

namespace Cimmerian::Visual {

inline MouseMoveEvent MouseMove(int x, int y) { return MouseMoveEvent {x, y}; }
inline MouseClickEvent MouseClick(int x, int y, int button = 1) { return MouseClickEvent {x, y, button}; }
inline MouseButtonPressEvent MouseButtonPress(int x, int y, int button = 1)
{
  return MouseButtonPressEvent {x, y, button};
}
inline MouseButtonReleaseEvent MouseButtonRelease(int button = 1) { return MouseButtonReleaseEvent {button}; }
inline MouseScrollEvent MouseScroll(int x, int y, int deltaX, int deltaY)
{
  return MouseScrollEvent {x, y, deltaX, deltaY};
}
inline KeyPressEvent KeyPress(int keyCode) { return KeyPressEvent {keyCode}; }
inline KeyReleaseEvent KeyRelease(int keyCode) { return KeyReleaseEvent {keyCode}; }

} // namespace Cimmerian::Visual
