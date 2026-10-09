// Measures how X11ScreenCapture's stability polling copes with a window that
// changes while it's being captured.
//
// It maps a window, then repaints it from a second X connection a set number
// of milliseconds after a capture starts, standing in for an app whose frame
// lands on screen asynchronously to the capture. For each delay it counts how
// often the capture came back with the repainted ("fresh") frame, comparing
// X11ScreenCapture::Capture() against a single XGetImage, which is what
// Capture() did before it polled. It then captures a window that never stops
// changing (Capture() should give up and warn) and one that blinks between
// two colours (where two captures can agree by chance).
//
// A measurement, not a test: the numbers depend on the machine, the build
// type and the window size, since one capture's duration sets how long the
// polling waits. Needs an X display:
//
//   xvfb-run -a ./build/x11_capture_stability [--trials N] [--size PIXELS]
//
// tools/x11-capture-stability/run-in-docker.sh builds and runs it in a Linux
// container, for machines without X11.

#include "cimmerian/visual/platform/x11-screen-capture.hpp"
#include "cimmerian/visual/screenshot.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::duration<double, std::milli>;

constexpr unsigned long kStaleColour = 0x0000FF; // the frame before the repaint
constexpr unsigned long kFreshColour = 0xFF0000; // the frame after it
constexpr int kRepaintDelaysMs[] = {0, 5, 10, 15, 20, 25, 30, 40, 60};

struct Options {
  int trials = 20;
  int windowSize = 400;
};

void Paint(Display* display, Window window, int windowSize, unsigned long colour)
{
  GC graphicsContext = XCreateGC(display, window, 0, nullptr);
  XSetForeground(display, graphicsContext, colour);
  XFillRectangle(display, window, graphicsContext, 0, 0, windowSize, windowSize);
  XFreeGC(display, graphicsContext);
  XSync(display, False);
}

bool IsFresh(unsigned char red, unsigned char blue)
{
  return red > 200 && blue < 50;
}

bool CentreIsFresh(const Cimmerian::Visual::Screenshot& screenshot)
{
  const std::size_t centreIndex =
      (static_cast<std::size_t>(screenshot.height / 2) * screenshot.width + screenshot.width / 2) * 4;
  return IsFresh(screenshot.pixels[centreIndex], screenshot.pixels[centreIndex + 2]);
}

// What Capture() did before it polled: one XGetImage, straight away.
bool SingleGetImageIsFresh(Display* display, Window window, int windowSize)
{
  XImage* image = XGetImage(display, window, 0, 0, windowSize, windowSize, AllPlanes, ZPixmap);
  const unsigned long centrePixel = XGetPixel(image, windowSize / 2, windowSize / 2);
  XDestroyImage(image);
  return IsFresh((centrePixel >> 16) & 0xFF, centrePixel & 0xFF);
}

// Runs Capture() and reports whether it logged its "did not stabilize"
// warning, by catching what it writes to stdout. That's exact where timing
// isn't: settling on the last attempt takes as long as giving up.
bool CaptureWarns(Cimmerian::Visual::X11ScreenCapture& capture, void* windowHandle)
{
  int pipeEnds[2];
  if (pipe(pipeEnds) != 0) {
    return false;
  }
  std::fflush(stdout);
  const int savedStdout = dup(STDOUT_FILENO);
  dup2(pipeEnds[1], STDOUT_FILENO);
  close(pipeEnds[1]);

  capture.Capture(windowHandle);

  std::fflush(stdout);
  dup2(savedStdout, STDOUT_FILENO);
  close(savedStdout);

  std::string captured;
  char buffer[512];
  for (ssize_t readCount; (readCount = read(pipeEnds[0], buffer, sizeof(buffer))) > 0;) {
    captured.append(buffer, static_cast<std::size_t>(readCount));
  }
  close(pipeEnds[0]);
  return captured.find("did not stabilize") != std::string::npos;
}

bool ParseOptions(int argc, char* argv[], Options* options)
{
  for (int i = 1; i < argc; ++i) {
    const bool hasValue = i + 1 < argc;
    if (std::strcmp(argv[i], "--trials") == 0 && hasValue) {
      options->trials = std::atoi(argv[++i]);
    }
    else if (std::strcmp(argv[i], "--size") == 0 && hasValue) {
      options->windowSize = std::atoi(argv[++i]);
    }
    else {
      std::fprintf(stderr, "usage: %s [--trials N] [--size PIXELS]\n", argv[0]);
      return false;
    }
  }
  return options->trials > 0 && options->windowSize > 0;
}

} // namespace

int main(int argc, char* argv[])
{
  Options options;
  if (!ParseOptions(argc, argv, &options)) {
    return 2;
  }

  XInitThreads();
  Display* display = XOpenDisplay(nullptr);
  Display* painterDisplay = XOpenDisplay(nullptr);
  if (!display || !painterDisplay) {
    std::fprintf(stderr, "x11_capture_stability: unable to open X display (is $DISPLAY set?)\n");
    return 1;
  }

  const int windowSize = options.windowSize;
  const Window window = XCreateSimpleWindow(
      display, DefaultRootWindow(display), 0, 0, windowSize, windowSize, 0, 0, kStaleColour
  );
  XSelectInput(display, window, StructureNotifyMask);
  XMapWindow(display, window);
  for (XEvent event;;) {
    XNextEvent(display, &event);
    if (event.type == MapNotify) {
      break;
    }
  }

  Cimmerian::Visual::X11ScreenCapture capture;
  void* windowHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(window));

  Paint(painterDisplay, window, windowSize, kStaleColour);
  Clock::time_point captureStart = Clock::now();
  capture.Capture(windowHandle);
  std::printf(
      "%dx%d window, %d trials per delay\n"
      "Capture() of an unchanging window: %.1f ms (capture, 16 ms wait, capture)\n\n",
      windowSize, windowSize, options.trials, Milliseconds(Clock::now() - captureStart).count()
  );

  std::printf("%-26s %-22s %s\n", "repaint lands after", "single XGetImage", "Capture()");
  int largestAlwaysCaughtDelayMs = -1;
  bool everyDelaySoFarCaught = true;
  for (const int repaintDelayMs : kRepaintDelaysMs) {
    int freshSingleCount = 0;
    int freshPolledCount = 0;

    for (const bool usePolling : {false, true}) {
      for (int trial = 0; trial < options.trials; ++trial) {
        Paint(painterDisplay, window, windowSize, kStaleColour);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        captureStart = Clock::now();
        std::thread presenter([&] {
          std::this_thread::sleep_until(captureStart + std::chrono::milliseconds(repaintDelayMs));
          Paint(painterDisplay, window, windowSize, kFreshColour);
        });

        if (usePolling) {
          freshPolledCount += CentreIsFresh(capture.Capture(windowHandle));
        }
        else {
          freshSingleCount += SingleGetImageIsFresh(display, window, windowSize);
        }
        presenter.join();
      }
    }

    if (everyDelaySoFarCaught && freshPolledCount == options.trials) {
      largestAlwaysCaughtDelayMs = repaintDelayMs;
    }
    else {
      everyDelaySoFarCaught = false;
    }

    std::printf(
        "%-26s %-22s %d/%d fresh\n", (std::to_string(repaintDelayMs) + " ms").c_str(),
        (std::to_string(freshSingleCount) + "/" + std::to_string(options.trials) + " fresh").c_str(),
        freshPolledCount, options.trials
    );
  }

  if (largestAlwaysCaughtDelayMs >= 0) {
    std::printf(
        "\nCapture() caught every repaint landing within %d ms of the call.\n",
        largestAlwaysCaughtDelayMs
    );
  }
  else {
    std::printf("\nCapture() missed a repaint even at 0 ms.\n");
  }

  // Never repeats a colour, so no two captures can match: Capture() should
  // exhaust its attempts and log a "did not stabilize" warning.
  std::printf("\nA window that never stops changing:\n");
  std::atomic<bool> isAnimating {true};
  std::thread animator([&] {
    unsigned long colour = 0x000100;
    while (isAnimating) {
      Paint(painterDisplay, window, windowSize, colour);
      colour = (colour + 0x010203) & 0xFFFFFF;
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  captureStart = Clock::now();
  const bool animatingWarned = CaptureWarns(capture, windowHandle);
  std::printf(
      "Capture() returned after %.1f ms, %s\n", Milliseconds(Clock::now() - captureStart).count(),
      animatingWarned ? "warning that it did not stabilize" : "WITHOUT a warning"
  );
  isAnimating = false;
  animator.join();

  // Two alternating colours: captures that happen to land on the same one
  // look stable, so Capture() can accept a frame of an animating window
  // without warning.
  std::printf("\nA window blinking between two colours every 3 ms:\n");
  std::atomic<bool> isBlinking {true};
  std::thread blinker([&] {
    bool showFresh = false;
    while (isBlinking) {
      Paint(painterDisplay, window, windowSize, showFresh ? kFreshColour : kStaleColour);
      showFresh = !showFresh;
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
  });
  int settledCount = 0;
  for (int trial = 0; trial < options.trials; ++trial) {
    settledCount += !CaptureWarns(capture, windowHandle);
  }
  isBlinking = false;
  blinker.join();
  std::printf(
      "Capture() accepted a frame as stable in %d/%d trials (gave up and warned in the rest)\n",
      settledCount, options.trials
  );

  XDestroyWindow(display, window);
  XCloseDisplay(painterDisplay);
  XCloseDisplay(display);
  return 0;
}
