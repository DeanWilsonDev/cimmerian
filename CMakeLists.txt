cmake_minimum_required(VERSION 3.25)

if(TARGET cimmerian)
  return()
endif()

project(
  cimmerian
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CIMMERIAN_IS_ROOT_PROJECT ON)
else()
  set(CIMMERIAN_IS_ROOT_PROJECT OFF)
endif()

# -------------------------
# Library target
# -------------------------
add_library(
  cimmerian STATIC
  src/test-group.cpp
  src/test-registry.cpp
  src/test-runner.cpp
  src/snapshot/snapshot-key.cpp
  src/snapshot/snapshot-run-mode.cpp
  src/snapshot/string-snapshot-store.cpp
  src/snapshot/hash-snapshot-store.cpp
  src/snapshot/inline-snapshot-rewriter.cpp
  src/visual/stb-impl.cpp
  src/visual/active-registries.cpp
  src/visual/navigation-driver.cpp
  src/visual/screenshot.cpp
  src/visual/image-diff.cpp
  src/visual/snapshot-store.cpp
  src/visual/visual-test-registry.cpp
  src/visual/visual-test-runner.cpp)

target_include_directories(
  cimmerian PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                   $<INSTALL_INTERFACE:include>)
target_include_directories(cimmerian PRIVATE src/visual)

target_compile_features(cimmerian PUBLIC cxx_std_20)
set_target_properties(cimmerian PROPERTIES C_EXTENSIONS OFF)

# -------------------------
# Visual regression: platform capture/injection
# -------------------------
# CIMMERIAN_VISUAL_PLATFORM lets consumers override platform autodetection (e.g.
# cross-compiling). Valid values: X11, Linux-uinput, Win32, macOS, None.
#
# Linux-uinput reuses X11's screen capture (unaffected by the gap below) but
# injects input via /dev/uinput instead of XTEST. XTEST-over-XWayland can report
# success without actually reaching the compositor's input pipeline on some
# Wayland compositors (see docs/cimmerian_wayland_xtest_injection_gap.md);
# uinput operates at the kernel input-device layer, below both X11 and Wayland,
# so it isn't subject to that gap. It requires /dev/uinput access (root, or a
# udev rule granting the `input` group), so it isn't the default even on Linux.
if(WIN32)
  set(_CIMMERIAN_VISUAL_PLATFORM_DEFAULT "Win32")
elseif(APPLE)
  set(_CIMMERIAN_VISUAL_PLATFORM_DEFAULT "macOS")
elseif(UNIX)
  set(_CIMMERIAN_VISUAL_PLATFORM_DEFAULT "X11")
else()
  set(_CIMMERIAN_VISUAL_PLATFORM_DEFAULT "None")
endif()

set(CIMMERIAN_VISUAL_PLATFORM
    "${_CIMMERIAN_VISUAL_PLATFORM_DEFAULT}"
    CACHE
      STRING
      "Visual regression platform backend (X11, Linux-uinput, Linux-auto, Win32, macOS, None)"
)
set_property(
  CACHE CIMMERIAN_VISUAL_PLATFORM
  PROPERTY STRINGS
           X11
           Linux-uinput
           Linux-auto
           Win32
           macOS
           None)

if(CIMMERIAN_VISUAL_PLATFORM STREQUAL "X11")
  find_package(X11 REQUIRED)
  if(NOT X11_XTest_FOUND)
    message(
      FATAL_ERROR
        "Cimmerian visual regression needs libXtst (X11 XTEST extension) headers/libs"
    )
  endif()

  target_sources(
    cimmerian
    PRIVATE src/visual/platform/x11-screen-capture.cpp
            src/visual/platform/x11-event-injector.cpp
            src/visual/platform/x11-window-lookup.cpp)
  target_include_directories(cimmerian PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(cimmerian PRIVATE ${X11_LIBRARIES} ${X11_Xtst_LIB})
  target_compile_definitions(cimmerian PUBLIC CIMMERIAN_VISUAL_PLATFORM_X11)

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-uinput")
  find_package(X11 REQUIRED)

  target_sources(
    cimmerian
    PRIVATE src/visual/platform/x11-screen-capture.cpp
            src/visual/platform/linux-uinput-event-injector.cpp
            src/visual/platform/x11-window-lookup.cpp)
  target_include_directories(cimmerian PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(cimmerian PRIVATE ${X11_LIBRARIES})
  target_compile_definitions(cimmerian
                             PUBLIC CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT)

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-auto")
  # Tries XTEST first, falls back to uinput at runtime if Probe() finds XTEST
  # non-functional on the current compositor (see
  # docs/cimmerian_wayland_xtest_injection_gap.md and
  # src/visual/platform/auto-linux-event-injector.cpp) - needs both backends'
  # dependencies present.
  find_package(X11 REQUIRED)
  if(NOT X11_XTest_FOUND)
    message(
      FATAL_ERROR
        "Cimmerian visual regression needs libXtst (X11 XTEST extension) headers/libs"
    )
  endif()

  target_sources(
    cimmerian
    PRIVATE src/visual/platform/x11-screen-capture.cpp
            src/visual/platform/x11-event-injector.cpp
            src/visual/platform/linux-uinput-event-injector.cpp
            src/visual/platform/auto-linux-event-injector.cpp
            src/visual/platform/x11-window-lookup.cpp)
  target_include_directories(cimmerian PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(cimmerian PRIVATE ${X11_LIBRARIES} ${X11_Xtst_LIB})
  target_compile_definitions(cimmerian
                             PUBLIC CIMMERIAN_VISUAL_PLATFORM_LINUX_AUTO)

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "Win32")
  target_sources(
    cimmerian
    PRIVATE src/visual/platform/win32-screen-capture.cpp
            src/visual/platform/win32-event-injector.cpp
            src/visual/platform/win32-window-lookup.cpp)
  target_link_libraries(cimmerian PRIVATE user32 gdi32)
  target_compile_definitions(cimmerian PUBLIC CIMMERIAN_VISUAL_PLATFORM_WIN32)

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "macOS")
  # macos-screen-capture.mm is Objective-C++: CGWindowListCreateImage and
  # CGDisplayCreateImage were obsoleted in the macOS 15 SDK, so screen capture
  # goes through ScreenCaptureKit instead.
  enable_language(OBJCXX)
  set_source_files_properties(src/visual/platform/macos-screen-capture.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")

  target_sources(
    cimmerian
    PRIVATE src/visual/platform/macos-screen-capture.mm
            src/visual/platform/macos-event-injector.cpp
            src/visual/platform/macos-window-lookup.cpp)
  find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
  find_library(SCREEN_CAPTURE_KIT_FRAMEWORK ScreenCaptureKit REQUIRED)
  target_link_libraries(cimmerian PRIVATE ${APPLICATION_SERVICES_FRAMEWORK}
                                          ${SCREEN_CAPTURE_KIT_FRAMEWORK})
  target_compile_definitions(cimmerian PUBLIC CIMMERIAN_VISUAL_PLATFORM_MACOS)
endif()

# -------------------------
# Output dirs (optional)
# -------------------------
set_target_properties(
  cimmerian
  PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
  set_target_properties(
    cimmerian
    PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER}
               "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}"
               LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER}
               "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}"
               RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER}
               "${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG}")
endforeach()

# -------------------------
# Warnings
# -------------------------
if(MSVC)
  target_compile_options(cimmerian PRIVATE /W4)
else()
  target_compile_options(cimmerian PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -------------------------
# Build define for shared
# -------------------------
if(BUILD_SHARED_LIBS)
  target_compile_definitions(cimmerian PRIVATE CIMMERIAN_BUILD_DLL)
endif()

# -------------------------
# Debug Option
# -------------------------

option(ENABLE_DEBUG "Compile in extra Debug logging" OFF)

if(ENABLE_DEBUG)
  add_definitions(-DENABLE_DEBUG)
endif()

# -------------------------
# Test executable
# -------------------------
# Cimmerian is a dev/test dependency: when it's pulled in as a subdirectory by
# another project (amanuensis, firefly, ...) we only want the library, never
# cimmerian's own test executable.
option(CIMMERIAN_BUILD_TESTS "Build cimmerian tests"
       ${CIMMERIAN_IS_ROOT_PROJECT})

if(CIMMERIAN_BUILD_TESTS)
  enable_language(CXX)

  # test/visual.test.cpp needs a real window to screenshot; it gets one from a
  # per-platform scratch-window backend (test/support/scratch-window-*),
  # selected the same way the library's own platform backend is above. Only add
  # visual.test.cpp (and its backend) where one exists.
  set(_CIMMERIAN_TEST_SOURCES test/test-main.cpp test/example.test.cpp
                              test/snapshot.test.cpp)

  if(CIMMERIAN_VISUAL_PLATFORM STREQUAL "X11"
     OR CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-uinput"
     OR CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-auto")
    # All three Linux backends use X11ScreenCapture (see the comment above
    # CIMMERIAN_VISUAL_PLATFORM_LINUX_UINPUT); only injection differs.
    list(APPEND _CIMMERIAN_TEST_SOURCES test/visual.test.cpp
         test/support/scratch-window-x11.cpp)
  elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "macOS")
    list(APPEND _CIMMERIAN_TEST_SOURCES test/visual.test.cpp
         test/support/scratch-window-macos.mm)
  else()
    message(
      STATUS
        "Cimmerian: no scratch-window test backend for CIMMERIAN_VISUAL_PLATFORM="
        "${CIMMERIAN_VISUAL_PLATFORM} yet; skipping visual.test.cpp self-tests")
  endif()

  add_executable(test_cimmerian ${_CIMMERIAN_TEST_SOURCES})
  target_link_libraries(test_cimmerian PRIVATE cimmerian)

  if(CIMMERIAN_VISUAL_PLATFORM STREQUAL "X11"
     OR CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-uinput"
     OR CIMMERIAN_VISUAL_PLATFORM STREQUAL "Linux-auto")
    target_include_directories(test_cimmerian PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(test_cimmerian PRIVATE ${X11_LIBRARIES})
  elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "macOS")
    set_source_files_properties(test/support/scratch-window-macos.mm
                                PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(test_cimmerian PRIVATE ${APPKIT_FRAMEWORK})
  endif()

  # tests are C++
  target_compile_features(test_cimmerian PRIVATE cxx_std_20)

  if(MSVC)
    target_compile_options(test_cimmerian PRIVATE /W4 /EHsc)
  else()
    target_compile_options(test_cimmerian PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# ------------------------
# Installation
# -------------------------
include(GNUInstallDirs)

install(
  TARGETS cimmerian
  EXPORT cimmerianTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  EXPORT cimmerianTargets
  FILE cimmerianTargets.cmake
  NAMESPACE cimmerian::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cimmerian)

# -------------------------
# Package config (enables find_package)
# -------------------------
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/CimmerianConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cimmerian)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/cimmerianConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cimmerian)
