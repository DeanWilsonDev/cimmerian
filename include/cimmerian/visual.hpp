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

#if defined(_WIN32) || defined(_WIN64)
#include "visual/platform/win32-event-injector.hpp"
#include "visual/platform/win32-screen-capture.hpp"
#elif defined(__APPLE__)
#include "visual/platform/macos-event-injector.hpp"
#include "visual/platform/macos-screen-capture.hpp"
#elif defined(__unix__) || defined(__linux__)
#include "visual/platform/x11-event-injector.hpp"
#include "visual/platform/x11-screen-capture.hpp"
#endif
