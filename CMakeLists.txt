cmake_minimum_required(VERSION 3.25)
project(Pagerank VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PR_BUILD_TESTS "Build unit tests"           ON)
option(PR_NATIVE      "Compile with -march=native" OFF)
option(PR_SANITIZE    "Enable ASan + UBSan"        OFF)

if(PR_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Library: all the logic, linked by the executable and by the tests
# ---------------------------------------------------------------------------
add_library(pr_core
  src/graph.cpp
  src/mtx_parser.cpp
  src/options.cpp
  src/pagerank.cpp
  src/report.cpp
)
target_include_directories(pr_core PUBLIC include)
target_compile_options(pr_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
if(PR_NATIVE)
  target_compile_options(pr_core PUBLIC -march=native)
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(pagerank app/main.cpp)
target_link_libraries(pagerank PRIVATE pr_core)

# ---------------------------------------------------------------------------
# Tests (GoogleTest, downloaded at configure time): one executable per
# tests/test_<name>.cpp
# ---------------------------------------------------------------------------
if(PR_BUILD_TESTS)
  enable_testing()

  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)

  foreach(name options parser graph pagerank)
    add_executable(test_${name} tests/test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE pr_core GTest::gtest_main)
    target_compile_definitions(test_${name} PRIVATE
      PR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    gtest_discover_tests(test_${name})
  endforeach()
endif()
