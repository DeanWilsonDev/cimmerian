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
# CIMMERIAN_VISUAL_PLATFORM lets consumers override platform autodetection
# (e.g. cross-compiling). Valid values: X11, Win32, macOS, None.
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
    CACHE STRING "Visual regression platform backend (X11, Win32, macOS, None)")
set_property(CACHE CIMMERIAN_VISUAL_PLATFORM PROPERTY STRINGS X11 Win32 macOS
                                                        None)

if(CIMMERIAN_VISUAL_PLATFORM STREQUAL "X11")
  find_package(X11 REQUIRED)
  if(NOT X11_XTest_FOUND)
    message(FATAL_ERROR "Cimmerian visual regression needs libXtst (X11 XTEST extension) headers/libs")
  endif()

  target_sources(cimmerian PRIVATE src/visual/platform/x11-screen-capture.cpp
                                    src/visual/platform/x11-event-injector.cpp)
  target_include_directories(cimmerian PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(cimmerian PRIVATE ${X11_LIBRARIES} ${X11_Xtst_LIB})

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "Win32")
  target_sources(cimmerian PRIVATE src/visual/platform/win32-screen-capture.cpp
                                    src/visual/platform/win32-event-injector.cpp)
  target_link_libraries(cimmerian PRIVATE user32 gdi32)

elseif(CIMMERIAN_VISUAL_PLATFORM STREQUAL "macOS")
  target_sources(cimmerian PRIVATE src/visual/platform/macos-screen-capture.cpp
                                    src/visual/platform/macos-event-injector.cpp)
  find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
  target_link_libraries(cimmerian PRIVATE ${APPLICATION_SERVICES_FRAMEWORK})
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
option(CIMMERIAN_BUILD_TESTS "Build cimmerian tests" ON)

if(CIMMERIAN_BUILD_TESTS)
  enable_language(CXX)
  add_executable(test_cimmerian test/test-main.cpp test/example.test.cpp
                                 test/snapshot.test.cpp test/visual.test.cpp)
  target_link_libraries(test_cimmerian PRIVATE cimmerian)

  if(CIMMERIAN_VISUAL_PLATFORM STREQUAL "X11")
    # test/visual.test.cpp creates its own scratch X11 window to screenshot.
    target_include_directories(test_cimmerian PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(test_cimmerian PRIVATE ${X11_LIBRARIES})
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
