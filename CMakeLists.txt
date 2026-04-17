cmake_minimum_required(VERSION 3.25)

project(
  Cimmerian
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------
# Library target
# -------------------------
add_library(cimmerian STATIC src/test-group.cpp src/test-registry.cpp
                              src/test-runner.cpp)

target_include_directories(
  cimmerian PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:include>)

target_compile_features(cimmerian PUBLIC cxx_std_20)
set_target_properties(cimmerian PROPERTIES C_EXTENSIONS OFF)

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
  target_compile_definitions(cimmerian PRIVATE UMBRA_BUILD_DLL)
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
option(CIMMERIAN_BUILD_TESTS "Build Cimmerian tests" ON)

if(CIMMERIAN_BUILD_TESTS)
  enable_language(CXX)
  add_executable(test_Cimmerian test/test-main.cpp test/example.test.cpp)
  target_link_libraries(test_Cimmerian PRIVATE cimmerian)

  # tests are C++
  target_compile_features(test_Cimmerian PRIVATE cxx_std_20)

  if(MSVC)
    target_compile_options(test_Cimmerian PRIVATE /W4 /EHsc)
  else()
    target_compile_options(test_Cimmerian PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# ------------------------
# Installation
# -------------------------
include(GNUInstallDirs)

install(
  TARGETS cimmerian
  EXPORT CimmerianTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  EXPORT CimmerianTargets
  FILE CimmerianTargets.cmake
  NAMESPACE Cimmerian::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cimmerian)

# -------------------------
# Package config (enables find_package)
# -------------------------
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/CimmerianConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cimmerian)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CimmerianConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cimmerian)
