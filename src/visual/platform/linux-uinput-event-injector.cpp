#include "cimmerian/visual/platform/linux-uinput-event-injector.hpp"
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include "cimmerian/test-log.hpp"

#if __has_include(<X11/Xlib.h>)
#include <X11/Xlib.h>
#define CIMMERIAN_UINPUT_HAVE_X11 1
#endif

namespace Cimmerian::Visual {

namespace {

// Standard `evdev` XKB rule offset: X11 keycodes are evdev codes + 8.
int X11KeycodeToEvdev(int x11Keycode)
{
  return x11Keycode - 8;
}

int ButtonToEvdev(int button)
{
  switch (button) {
    case 2: return BTN_MIDDLE;
    case 3: return BTN_RIGHT;
    default: return BTN_LEFT;
  }
}

void DetectScreenSize(int& width, int& height)
{
  if (const char* envWidth = std::getenv("CIMMERIAN_VISUAL_SCREEN_WIDTH")) {
    width = std::atoi(envWidth);
  }
  if (const char* envHeight = std::getenv("CIMMERIAN_VISUAL_SCREEN_HEIGHT")) {
    height = std::atoi(envHeight);
  }
  if (width > 0 && height > 0) {
    return;
  }

#if defined(CIMMERIAN_UINPUT_HAVE_X11)
  if (Display* dpy = XOpenDisplay(nullptr)) {
    if (width <= 0) {
      width = DisplayWidth(dpy, DefaultScreen(dpy));
    }
    if (height <= 0) {
      height = DisplayHeight(dpy, DefaultScreen(dpy));
    }
    XCloseDisplay(dpy);
  }
#endif

  if (width <= 0) {
    width = 1920;
  }
  if (height <= 0) {
    height = 1080;
  }
}

} // namespace

LinuxUinputEventInjector::LinuxUinputEventInjector(int width, int height)
    : fd(-1)
    , screenWidth(width)
    , screenHeight(height)
    , functional(true)
{
  DetectScreenSize(this->screenWidth, this->screenHeight);

  this->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (this->fd < 0) {
    throw std::runtime_error(
        "LinuxUinputEventInjector: unable to open /dev/uinput (need root, or "
        "a udev rule granting the `input` group write access)"
    );
  }

  ioctl(this->fd, UI_SET_EVBIT, EV_KEY);
  ioctl(this->fd, UI_SET_EVBIT, EV_ABS);
  ioctl(this->fd, UI_SET_EVBIT, EV_REL);
  ioctl(this->fd, UI_SET_EVBIT, EV_SYN);

  ioctl(this->fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(this->fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(this->fd, UI_SET_KEYBIT, BTN_MIDDLE);
  // Broad keyboard coverage: every KEY_* code covering standard keyboards.
  for (int code = KEY_ESC; code <= KEY_MICMUTE; ++code) {
    ioctl(this->fd, UI_SET_KEYBIT, code);
  }

  ioctl(this->fd, UI_SET_ABSBIT, ABS_X);
  ioctl(this->fd, UI_SET_ABSBIT, ABS_Y);
  ioctl(this->fd, UI_SET_RELBIT, REL_WHEEL);
  ioctl(this->fd, UI_SET_RELBIT, REL_HWHEEL);
  ioctl(this->fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

  struct uinput_setup setup {};
  setup.id.bustype = BUS_VIRTUAL;
  setup.id.vendor = 0x0;
  setup.id.product = 0x0;
  setup.id.version = 1;
  std::strncpy(setup.name, "Cimmerian Virtual Input", sizeof(setup.name) - 1);

  if (ioctl(this->fd, UI_DEV_SETUP, &setup) < 0) {
    close(this->fd);
    throw std::runtime_error("LinuxUinputEventInjector: UI_DEV_SETUP failed");
  }

  struct uinput_abs_setup absX {};
  absX.code = ABS_X;
  absX.absinfo.minimum = 0;
  absX.absinfo.maximum = this->screenWidth > 0 ? this->screenWidth - 1 : 0;
  if (ioctl(this->fd, UI_ABS_SETUP, &absX) < 0) {
    close(this->fd);
    throw std::runtime_error("LinuxUinputEventInjector: UI_ABS_SETUP (X) failed");
  }

  struct uinput_abs_setup absY {};
  absY.code = ABS_Y;
  absY.absinfo.minimum = 0;
  absY.absinfo.maximum = this->screenHeight > 0 ? this->screenHeight - 1 : 0;
  if (ioctl(this->fd, UI_ABS_SETUP, &absY) < 0) {
    close(this->fd);
    throw std::runtime_error("LinuxUinputEventInjector: UI_ABS_SETUP (Y) failed");
  }

  if (ioctl(this->fd, UI_DEV_CREATE) < 0) {
    close(this->fd);
    throw std::runtime_error("LinuxUinputEventInjector: UI_DEV_CREATE failed");
  }

  // The compositor/libinput needs a moment to enumerate the new device
  // before it will act on events sent to it; sending immediately after
  // UI_DEV_CREATE is a well-known way to have the first few events dropped.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

LinuxUinputEventInjector::~LinuxUinputEventInjector()
{
  if (this->fd >= 0) {
    ioctl(this->fd, UI_DEV_DESTROY);
    close(this->fd);
  }
}

bool LinuxUinputEventInjector::Probe()
{
#if defined(CIMMERIAN_UINPUT_HAVE_X11)
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    // Can't check by reading the real pointer position; don't block on it.
    this->functional = true;
    return this->functional;
  }

  Window root = DefaultRootWindow(dpy);
  Window childReturn;
  int origX = 0, origY = 0, winXReturn = 0, winYReturn = 0;
  unsigned int maskReturn = 0;
  XQueryPointer(dpy, root, &childReturn, &childReturn, &origX, &origY, &winXReturn, &winYReturn, &maskReturn);

  const int probeX = (origX + this->screenWidth / 2) % std::max(this->screenWidth, 1);
  const int probeY = (origY + this->screenHeight / 2) % std::max(this->screenHeight, 1);
  this->MoveTo(probeX, probeY);
  // Give the compositor/seat a moment to consume the event before checking.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  int movedX = 0, movedY = 0;
  XQueryPointer(dpy, root, &childReturn, &childReturn, &movedX, &movedY, &winXReturn, &winYReturn, &maskReturn);

  this->functional = (movedX != origX) || (movedY != origY);

  // Restore the original pointer position regardless of outcome - this probe
  // shouldn't leave a visible side effect.
  this->MoveTo(origX, origY);
  XCloseDisplay(dpy);

  if (!this->functional) {
    TEST_LOG_WARN(
        "LinuxUinputEventInjector: /dev/uinput device created successfully "
        "but events aren't reaching the real pointer on this session - "
        "possibly a logind/seat-assignment gap (see "
        "docs/cimmerian_uinput_no_functional_check_gap.md). SEND() in "
        "visual tests will silently no-op."
    );
  }
#else
  // No X11 available to read the real pointer position back - nothing to
  // compare against, so stay optimistic.
  this->functional = true;
#endif

  return this->functional;
}

void LinuxUinputEventInjector::MoveTo(int x, int y)
{
  const int clampedX = x < 0 ? 0 : (x >= this->screenWidth ? this->screenWidth - 1 : x);
  const int clampedY = y < 0 ? 0 : (y >= this->screenHeight ? this->screenHeight - 1 : y);

  struct input_event ev {};
  ev.type = EV_ABS;
  ev.code = ABS_X;
  ev.value = clampedX;
  write(this->fd, &ev, sizeof(ev));

  ev.code = ABS_Y;
  ev.value = clampedY;
  write(this->fd, &ev, sizeof(ev));

  this->SyncReport();
}

void LinuxUinputEventInjector::EmitKey(int code, int value)
{
  struct input_event ev {};
  ev.type = EV_KEY;
  ev.code = static_cast<unsigned short>(code);
  ev.value = value;
  write(this->fd, &ev, sizeof(ev));
  this->SyncReport();
}

void LinuxUinputEventInjector::EmitScroll(int code, int value)
{
  struct input_event ev {};
  ev.type = EV_REL;
  ev.code = static_cast<unsigned short>(code);
  ev.value = value;
  write(this->fd, &ev, sizeof(ev));
  this->SyncReport();
}

void LinuxUinputEventInjector::SyncReport()
{
  struct input_event ev {};
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  write(this->fd, &ev, sizeof(ev));
}

void LinuxUinputEventInjector::Inject(const UIEvent& event)
{
  std::visit(
      [this](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, MouseMoveEvent>) {
          this->MoveTo(e.x, e.y);
        }
        else if constexpr (std::is_same_v<T, MouseClickEvent>) {
          this->MoveTo(e.x, e.y);
          const int btn = ButtonToEvdev(e.button);
          this->EmitKey(btn, 1);
          this->EmitKey(btn, 0);
        }
        else if constexpr (std::is_same_v<T, MouseButtonPressEvent>) {
          this->MoveTo(e.x, e.y);
          this->EmitKey(ButtonToEvdev(e.button), 1);
        }
        else if constexpr (std::is_same_v<T, MouseButtonReleaseEvent>) {
          this->EmitKey(ButtonToEvdev(e.button), 0);
        }
        else if constexpr (std::is_same_v<T, MouseScrollEvent>) {
          this->MoveTo(e.x, e.y);
          const int verticalStep = e.deltaY > 0 ? 1 : -1;
          for (int i = 0; i < std::abs(e.deltaY); ++i) {
            this->EmitScroll(REL_WHEEL, verticalStep);
          }
          const int horizontalStep = e.deltaX > 0 ? 1 : -1;
          for (int i = 0; i < std::abs(e.deltaX); ++i) {
            this->EmitScroll(REL_HWHEEL, horizontalStep);
          }
        }
        else if constexpr (std::is_same_v<T, KeyPressEvent>) {
          this->EmitKey(X11KeycodeToEvdev(e.keyCode), 1);
        }
        else if constexpr (std::is_same_v<T, KeyReleaseEvent>) {
          this->EmitKey(X11KeycodeToEvdev(e.keyCode), 0);
        }
        else if constexpr (std::is_same_v<T, WaitEvent>) {
          // Handled by VisualTestRunner::SendEvent before it reaches here.
        }
      },
      event
  );
}

} // namespace Cimmerian::Visual
