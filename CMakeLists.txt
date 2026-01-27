cmake_minimum_required(VERSION 3.25)

project(
  UmbraTest
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------
# Library target
# -------------------------
add_library(umbra_test STATIC src/test-group.cpp src/test-registry.cpp
                              src/test-runner.cpp)

target_include_directories(
  umbra_test PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:include>)

target_compile_features(umbra_test PUBLIC cxx_std_20)
set_target_properties(umbra_test PROPERTIES C_EXTENSIONS OFF)

# -------------------------
# Output dirs (optional)
# -------------------------
set_target_properties(
  umbra_test
  PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
  set_target_properties(
    umbra_test
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
  target_compile_options(umbra_test PRIVATE /W4)
else()
  target_compile_options(umbra_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -------------------------
# Build define for shared
# -------------------------
if(BUILD_SHARED_LIBS)
  target_compile_definitions(umbra_test PRIVATE UMBRA_BUILD_DLL)
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
option(UMBRATEST_BUILD_TESTS "Build UmbraTest tests" ON)

if(UMBRATEST_BUILD_TESTS)
  enable_language(CXX)
  add_executable(test_UmbraTest test/test-main.cpp test/example.test.cpp)
  target_link_libraries(test_UmbraTest PRIVATE umbra_test)

  # tests are C++
  target_compile_features(test_UmbraTest PRIVATE cxx_std_20)

  if(MSVC)
    target_compile_options(test_UmbraTest PRIVATE /W4 /EHsc)
  else()
    target_compile_options(test_UmbraTest PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# ------------------------
# Installation
# -------------------------
include(GNUInstallDirs)

install(
  TARGETS umbra_test
  EXPORT UmbraTestTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  EXPORT UmbraTestTargets
  FILE UmbraTestTargets.cmake
  NAMESPACE Umbra::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UmbraTest)
