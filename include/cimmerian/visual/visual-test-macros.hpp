#pragma once

#include "../test-macro-helpers.hpp"
#include "ui-event.hpp"
#include "visual-test-registry.hpp"
#include "visual-test-runner.hpp"

/* ################################# */
/* ========= FRAMEWORK STATE ======= */
/* ################################# */

inline Cimmerian::Visual::VisualTestGroup* _visual_test_group = nullptr;

/* ################################# */
/* ========= VISUAL_DESCRIBE: ====== */
/* ################################# */

#define VISUAL_DESCRIBE(group_name, window_handle, BODY)                                          \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_visual_desc_reg_, __COUNTER__) = []() {         \
    Cimmerian::Visual::VisualTestRegistry& _registry =                                             \
        Cimmerian::Visual::VisualTestRegistry::GetInstance();                                       \
    Cimmerian::Visual::VisualTestGroup* _parent =                                                   \
        (_visual_test_group != nullptr) ? _visual_test_group : _registry.GetRootGroup();            \
    Cimmerian::Visual::VisualTestGroup* _child =                                                    \
        _registry.GetChildGroup(_parent, (group_name), (window_handle));                            \
    Cimmerian::Visual::VisualTestGroup* _saved = _visual_test_group;                                \
    _visual_test_group = _child;                                                                    \
    do                                                                                              \
      BODY while (0);                                                                              \
    _visual_test_group = _saved;                                                                    \
    return true;                                                                                    \
  }();

/* ################################# */
/* =========== VISUAL_TEST: ======== */
/* ################################# */

#define VISUAL_TEST(testName, ...)                                                                \
  CIMMERIAN_MAYBE_UNUSED static const bool MACRO_CAT(_visual_test_reg_, __COUNTER__) = [=]() {        \
    Cimmerian::Visual::VisualTestRegistry& _registry =                                             \
        Cimmerian::Visual::VisualTestRegistry::GetInstance();                                       \
    Cimmerian::Visual::VisualTestGroup* _group =                                                    \
        _visual_test_group ? _visual_test_group : _registry.GetRootGroup();                        \
    _registry.RegisterTest(                                                                        \
        _group, (testName),                                                                        \
        +[](void* user) {                                                                          \
          (void)user;                                                                              \
          __VA_ARGS__                                                                              \
        }                                                                                          \
    );                                                                                             \
    return true;                                                                                    \
  }();

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
