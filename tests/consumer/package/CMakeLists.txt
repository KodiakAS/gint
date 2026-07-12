cmake_minimum_required(VERSION 3.13)
project(gint_package_consumer LANGUAGES CXX)

if(DEFINED GINT_FIND_VERSION)
    if(GINT_FIND_EXACT)
        find_package(gint ${GINT_FIND_VERSION} EXACT CONFIG REQUIRED)
    else()
        find_package(gint ${GINT_FIND_VERSION} CONFIG REQUIRED)
    endif()
else()
    find_package(gint CONFIG REQUIRED)
endif()

if(NOT TARGET gint::gint OR NOT TARGET gint::checked)
    message(FATAL_ERROR "installed gint package did not export the expected targets")
endif()

foreach(gint_target IN ITEMS gint::gint gint::checked)
    get_target_property(gint_target_type ${gint_target} TYPE)
    if(NOT gint_target_type STREQUAL "INTERFACE_LIBRARY")
        message(FATAL_ERROR "${gint_target} is '${gint_target_type}', expected INTERFACE_LIBRARY")
    endif()
endforeach()

get_target_property(gint_compile_features gint::gint INTERFACE_COMPILE_FEATURES)
list(FIND gint_compile_features cxx_std_11 gint_cxx11_position)
if(gint_cxx11_position EQUAL -1)
    message(FATAL_ERROR "gint::gint does not export the cxx_std_11 requirement")
endif()

get_target_property(gint_include_dirs gint::gint INTERFACE_INCLUDE_DIRECTORIES)
if(NOT gint_include_dirs)
    message(FATAL_ERROR "gint::gint does not export an include directory")
endif()
foreach(gint_include_dir IN LISTS gint_include_dirs)
    if(NOT IS_DIRECTORY "${gint_include_dir}")
        message(FATAL_ERROR "gint::gint exports a missing include directory: ${gint_include_dir}")
    endif()
endforeach()

get_target_property(gint_checked_definitions gint::checked INTERFACE_COMPILE_DEFINITIONS)
list(FIND gint_checked_definitions GINT_ENABLE_DIVZERO_CHECKS gint_checked_definition_position)
if(gint_checked_definition_position EQUAL -1)
    message(FATAL_ERROR "gint::checked does not export GINT_ENABLE_DIVZERO_CHECKS")
endif()

get_target_property(gint_checked_links gint::checked INTERFACE_LINK_LIBRARIES)
list(FIND gint_checked_links gint::gint gint_checked_link_position)
if(gint_checked_link_position EQUAL -1)
    message(FATAL_ERROR "gint::checked does not transitively link gint::gint")
endif()

if(DEFINED GINT_EXPECT_VERSION AND NOT gint_VERSION VERSION_EQUAL GINT_EXPECT_VERSION)
    message(FATAL_ERROR
        "installed package reports version '${gint_VERSION}', expected '${GINT_EXPECT_VERSION}'"
    )
endif()

add_executable(gint_package_consumer main.cpp)
target_link_libraries(gint_package_consumer PRIVATE gint::gint)
set_target_properties(gint_package_consumer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

if(DEFINED GINT_EXPECT_VERSION)
    target_compile_definitions(gint_package_consumer PRIVATE
        GINT_EXPECT_VERSION_MAJOR=${gint_VERSION_MAJOR}
        GINT_EXPECT_VERSION_MINOR=${gint_VERSION_MINOR}
        GINT_EXPECT_VERSION_PATCH=${gint_VERSION_PATCH}
    )
endif()
