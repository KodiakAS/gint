cmake_minimum_required(VERSION 3.13)
project(gint_package_consumer LANGUAGES CXX)

find_package(gint CONFIG REQUIRED)

if(NOT TARGET gint::gint OR NOT TARGET gint::checked)
    message(FATAL_ERROR "installed gint package did not export the expected targets")
endif()

add_executable(gint_package_consumer main.cpp)
target_link_libraries(gint_package_consumer PRIVATE gint::gint)
set_target_properties(gint_package_consumer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
