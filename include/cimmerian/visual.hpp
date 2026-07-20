#pragma once

#include "visual/i-event-injector.hpp"
#include "visual/i-screen-capture.hpp"
#include "visual/image-diff.hpp"
#include "visual/screenshot.hpp"
#include "visual/snapshot-store.hpp"
#include "visual/ui-event.hpp"
#include "visual/visual-test-case.hpp"
#include "visual/visual-test-macros.hpp"
#include "visual/visual-test-registry.hpp"
#include "visual/visual-test-runner.hpp"

// Selected by the CIMMERIAN_VISUAL_PLATFORM_* compile definitions CMake sets
// based on the CIMMERIAN_VISUAL_PLATFORM cache variable, not raw OS macros -
// that's what lets e.g. Linux-uinput opt out of the X11 event injector while
// still building on a Linux/UNIX host.
#if defined(CIMMERIAN_VISUAL_PLATFORM_WIN32)
#include "visual/platform/win32-event-injector.hpp"
#include "visual/platform/win32-screen-capture.hpp"
#elif defined(CIMMERIAN_VISUAL_PLATFORM_MACOS)
#include "visual/platform/macos-event-injector.hpp"
#include "visual/platform/macos-screen-capture.hpp"
#elif defined(CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT)
#include "visual/platform/linux-uinput-event-injector.hpp"
#include "visual/platform/x11-screen-capture.hpp"
#elif defined(CIMMERIAN_VISUAL_PLATFORM_X11)
#include "visual/platform/x11-event-injector.hpp"
#include "visual/platform/x11-screen-capture.hpp"
#endif
