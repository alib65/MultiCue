cmake_minimum_required(VERSION 3.25)

project(MultiCue VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(MULTICUE_BUILD_TESTS "Build the Catch2 test suites" ON)
option(MULTICUE_ENABLE_ASAN "Build with AddressSanitizer + UBSan" OFF)

if(MULTICUE_BUILD_TESTS)
  include(CTest)
  find_package(Catch2 3 REQUIRED)
  include(Catch)
endif()

find_package(Threads REQUIRED)

add_library(multicue_warnings INTERFACE)
if(MSVC)
  target_compile_options(multicue_warnings INTERFACE /W4)
else()
  target_compile_options(multicue_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(MULTICUE_ENABLE_ASAN)
  add_library(multicue_sanitizers INTERFACE)
  target_compile_options(multicue_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(multicue_sanitizers INTERFACE -fsanitize=address,undefined)
else()
  add_library(multicue_sanitizers INTERFACE)
endif()

add_subdirectory(mc_core)
add_subdirectory(mc_dsp)
add_subdirectory(mc_engine)
add_subdirectory(mc_video)
add_subdirectory(apps/multicue-cli)
add_subdirectory(apps/multicue)
