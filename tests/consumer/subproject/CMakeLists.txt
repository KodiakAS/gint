cmake_minimum_required(VERSION 3.13)
project(gint_subproject_consumer LANGUAGES CXX)

if(NOT GINT_SOURCE_DIR)
    message(FATAL_ERROR "GINT_SOURCE_DIR is required")
endif()

set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
add_subdirectory(${GINT_SOURCE_DIR} gint-src)

if(NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(FATAL_ERROR "gint changed its parent project's CMAKE_BUILD_TYPE")
endif()
if(TARGET gint_tests OR TARGET perf_benchmark_int256)
    message(FATAL_ERROR "gint enabled development targets as a subproject")
endif()
if(NOT TARGET gint::gint OR NOT TARGET gint::checked)
    message(FATAL_ERROR "gint subproject did not define the expected targets")
endif()

add_executable(gint_subproject_consumer main.cpp)
target_link_libraries(gint_subproject_consumer PRIVATE gint::gint)
set_target_properties(gint_subproject_consumer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
