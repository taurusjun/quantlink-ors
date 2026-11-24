cmake_minimum_required(VERSION 3.25)
project(quantlink_ors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Root-level so `ctest --test-dir build` finds the ctp/ tests.
enable_testing()

add_subdirectory(ctp)
