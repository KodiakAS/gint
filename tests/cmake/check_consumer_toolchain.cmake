# Exercise the production snapshot and runner with a real child compilation.
# This probe has no GTest/fmt dependency and runs on the CMake 3.13 lane too.
set(probe "${GINT_BINARY_DIR}/consumer-forwarding-regression")
file(REMOVE_RECURSE "${probe}")
file(MAKE_DIRECTORY "${probe}/parent" "${probe}/source/tests/consumer/subproject")
file(WRITE "${probe}/parent/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.13)\n"
    "project(consumer_toolchain_parent LANGUAGES CXX)\n"
    "include([==[${GINT_SOURCE_DIR}/tests/cmake/prepare_consumer.cmake]==])\n")
file(WRITE "${probe}/toolchain.cmake" [====[
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MY_REQUIRED MY_EMPTY MY_BOOL MY_UNSET)
if(NOT "${MY_REQUIRED}" STREQUAL [===[SDK root;]==];quoted "value"]===])
    message(FATAL_ERROR "declared toolchain input was lost or corrupted")
endif()
if(NOT DEFINED MY_EMPTY OR NOT "${MY_EMPTY}" STREQUAL "" OR
   NOT DEFINED MY_BOOL OR MY_BOOL OR DEFINED MY_UNSET)
    message(FATAL_ERROR "toolchain input definedness was changed")
endif()
]====])
file(WRITE "${probe}/seed.cmake" [====[
set(MY_REQUIRED [===[SDK root;]==];quoted "value"]===] CACHE STRING "")
set(MY_EMPTY "" CACHE STRING "")
set(MY_BOOL OFF CACHE BOOL "")
set(CMAKE_BUILD_TYPE Release CACHE STRING "")
set(CMAKE_CXX_FLAGS "-DGINT_GLOBAL_FLAG=150" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE "-DGINT_RELEASE_FLAG=150" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-DGINT_LINK_FLAG=150" CACHE STRING "")
]====])
file(WRITE "${probe}/source/tests/consumer/subproject/CMakeLists.txt" [==[
cmake_minimum_required(VERSION 3.13)
project(consumer_toolchain_child LANGUAGES CXX)
include("${GINT_COMPILER_CHECK}")
set(CMAKE_VERBOSE_MAKEFILE TRUE)
add_executable(probe main.cpp)
set_target_properties(probe PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
]==])
file(WRITE "${probe}/source/tests/consumer/subproject/main.cpp" [==[
#if !defined(GINT_GLOBAL_FLAG) || GINT_GLOBAL_FLAG != 150
#error parent global flags did not reach the actual compiler
#endif
#if !defined(GINT_RELEASE_FLAG) || GINT_RELEASE_FLAG != 150
#error parent Release configuration was not selected for compilation
#endif
int main() { return 0; }
]==])
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${probe}/parent" -B "${probe}/build"
        -G "${GINT_TEST_GENERATOR}" -C "${probe}/seed.cmake"
        "-DCMAKE_CXX_COMPILER=${GINT_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${probe}/toolchain.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to configure toolchain probe parent:\n${stdout}\n${stderr}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DGINT_CONSUMER_KIND=subproject
        "-DGINT_SOURCE_DIR=${probe}/source"
        "-DGINT_BINARY_DIR=${probe}/build"
        "-DGINT_CONSUMER_GENERATOR=${GINT_TEST_GENERATOR}"
        -DGINT_CONSUMER_CONFIG=Release
        -P "${GINT_SOURCE_DIR}/tests/cmake/run_consumer.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "toolchain forwarding failed:\n${stdout}\n${stderr}")
endif()
# Makefiles print the real link command; Ninja may abbreviate it. Compile-time
# checks above verify active flags for either generator, without cache-only tests.
if(GINT_TEST_GENERATOR STREQUAL "Unix Makefiles")
    string(FIND "${stdout}" "-DGINT_LINK_FLAG=150" link_flag_position)
    if(link_flag_position EQUAL -1)
        message(FATAL_ERROR "Release linker flags were not used:\n${stdout}")
    endif()
endif()
