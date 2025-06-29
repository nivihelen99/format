cmake_minimum_required(VERSION 3.14)
project(CompatFormat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Main Library (Header-only, but define an interface target) ---
add_library(compat_format INTERFACE)
target_include_directories(compat_format INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# --- Example Executable (using main.cpp) ---
add_executable(example main.cpp)
target_link_libraries(example PRIVATE compat_format)

# --- Google Test ---
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip # Or a specific commit/tag
  # GIT_REPOSITORY https://github.com/google/googletest.git
  # GIT_TAG main # release-1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# --- Test Executable ---
enable_testing()

add_executable(gtest_runner tests/gtest_main.cpp)
target_link_libraries(gtest_runner PRIVATE compat_format GTest::gtest GTest::gtest_main)

# Include GTest in "all" builds & add test to CTest
include(GoogleTest)
gtest_discover_tests(gtest_runner)

# Optional: Add a top-level tests target that builds and runs tests
# add_custom_target(run_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
# DEPENDS gtest_runner)

message(STATUS "CompatFormat Project Configured")
message(STATUS "  Library target: compat_format")
message(STATUS "  Example target: example (builds main.cpp)")
message(STATUS "  Test target:    gtest_runner (builds tests/gtest_main.cpp)")
message(STATUS "To build and run tests: ")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  cmake --build .")
message(STATUS "  ctest") # or ./gtest_runner directly
message(STATUS "To build example: cmake --build . --target example")
