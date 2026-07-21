#pragma once

#include <chrono>
#include <variant>
#include <vector>

namespace Cimmerian::Visual {

struct MouseMoveEvent {
  int x;
  int y;
};
struct MouseClickEvent {
  int x;
  int y;
  int button; // 1=left 2=mid 3=right
};
struct MouseButtonPressEvent {
  int x;
  int y;
  int button; // 1=left 2=mid 3=right
};
struct MouseButtonReleaseEvent {
  int button; // no x/y -- targets whatever button is currently down, same
              // convention KeyReleaseEvent already uses
};
struct MouseScrollEvent {
  int x;
  int y;
  int deltaX;
  int deltaY;
};
struct KeyPressEvent {
  int keyCode;
};
struct KeyReleaseEvent {
  int keyCode;
};
struct WaitEvent {
  std::chrono::milliseconds duration;
};

using UIEvent = std::variant<
    MouseMoveEvent,
    MouseClickEvent,
    MouseButtonPressEvent,
    MouseButtonReleaseEvent,
    MouseScrollEvent,
    KeyPressEvent,
    KeyReleaseEvent,
    WaitEvent>;

struct EventSequence {
  std::vector<UIEvent> events;

  EventSequence& MouseMove(int x, int y)
  {
    this->events.push_back(MouseMoveEvent {x, y});
    return *this;
  }

  EventSequence& Click(int x, int y, int button = 1)
  {
    this->events.push_back(MouseClickEvent {x, y, button});
    return *this;
  }

  EventSequence& MouseButtonPress(int x, int y, int button = 1)
  {
    this->events.push_back(MouseButtonPressEvent {x, y, button});
    return *this;
  }

  EventSequence& MouseButtonRelease(int button = 1)
  {
    this->events.push_back(MouseButtonReleaseEvent {button});
    return *this;
  }

  EventSequence& Scroll(int x, int y, int deltaX, int deltaY)
  {
    this->events.push_back(MouseScrollEvent {x, y, deltaX, deltaY});
    return *this;
  }

  EventSequence& KeyPress(int keyCode)
  {
    this->events.push_back(KeyPressEvent {keyCode});
    return *this;
  }

  EventSequence& KeyRelease(int keyCode)
  {
    this->events.push_back(KeyReleaseEvent {keyCode});
    return *this;
  }

  EventSequence& Wait(std::chrono::milliseconds ms)
  {
    this->events.push_back(WaitEvent {ms});
    return *this;
  }
};

} // namespace Cimmerian::Visual
