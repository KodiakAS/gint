# Test-only C++ toolchain transport. Included after project() has resolved the
# compiler and environment-initialized flags; not part of the installed package.
function(gint_consumer_quote output value)
    set(equals "==")
    string(FIND "${value}" "]${equals}]" delimiter_position)
    while(NOT delimiter_position EQUAL -1)
        string(APPEND equals "=")
        string(FIND "${value}" "]${equals}]" delimiter_position)
    endwhile()
    set(${output} "[${equals}[${value}]${equals}]" PARENT_SCOPE)
endfunction()

# Snapshot the selected toolchain as a cache preload file. Bracket quoting
# preserves paths with spaces and list-valued architecture settings.
set(GINT_CONSUMER_TOOLCHAIN_VARIABLES
    CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ARG1 CMAKE_TOOLCHAIN_FILE
    CMAKE_TRY_COMPILE_TARGET_TYPE CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    CMAKE_CXX_STANDARD_LIBRARIES CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES CMAKE_GENERATOR_INSTANCE CMAKE_MAKE_PROGRAM
    CMAKE_CXX_COMPILER_TARGET CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN
    CMAKE_SYSROOT CMAKE_SYSROOT_COMPILE CMAKE_SYSROOT_LINK
    CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
)
# Custom toolchain inputs follow CMake's documented propagation declaration.
# Do not clone the parent cache: it includes project options and search results.
list(APPEND GINT_CONSUMER_TOOLCHAIN_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES})
if(CMAKE_CROSSCOMPILING)
    list(APPEND GINT_CONSUMER_TOOLCHAIN_VARIABLES
        CMAKE_SYSTEM_NAME CMAKE_SYSTEM_VERSION CMAKE_SYSTEM_PROCESSOR)
endif()
set(gint_consumer_flag_variables CMAKE_CXX_FLAGS CMAKE_CXX_LINK_FLAGS CMAKE_EXE_LINKER_FLAGS)
set(gint_consumer_configurations Debug Release RelWithDebInfo MinSizeRel
    ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
list(REMOVE_DUPLICATES gint_consumer_configurations)
foreach(configuration IN LISTS gint_consumer_configurations)
    string(TOUPPER "${configuration}" configuration_upper)
    list(APPEND gint_consumer_flag_variables
        CMAKE_CXX_FLAGS_${configuration_upper}
        CMAKE_CXX_LINK_FLAGS_${configuration_upper}
        CMAKE_EXE_LINKER_FLAGS_${configuration_upper})
endforeach()
list(REMOVE_DUPLICATES gint_consumer_flag_variables)
list(APPEND GINT_CONSUMER_TOOLCHAIN_VARIABLES ${gint_consumer_flag_variables})
list(REMOVE_DUPLICATES GINT_CONSUMER_TOOLCHAIN_VARIABLES)
set(gint_consumer_toolchain "${CMAKE_CURRENT_BINARY_DIR}/consumer-toolchain.cmake")
file(WRITE "${gint_consumer_toolchain}" "# Generated consumer toolchain cache.\n")
foreach(variable IN LISTS GINT_CONSUMER_TOOLCHAIN_VARIABLES)
    if(DEFINED ${variable})
        gint_consumer_quote(quoted_name "${variable}")
        gint_consumer_quote(quoted_value "${${variable}}")
        get_property(cache_type CACHE "${variable}" PROPERTY TYPE)
        if(NOT cache_type OR cache_type STREQUAL "UNINITIALIZED")
            set(cache_type STRING)
        endif()
        file(APPEND "${gint_consumer_toolchain}"
            "set(${quoted_name} ${quoted_value} CACHE ${cache_type} \"Parent toolchain\" FORCE)\n")
    endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/consumer-compiler-check.cmake"
    "# Generated checks for selected compiler, flags and build configuration.\n")
set(gint_consumer_checked_variables
    CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ID CMAKE_CXX_COMPILER_VERSION
    ${gint_consumer_flag_variables} CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
foreach(variable IN LISTS gint_consumer_checked_variables)
    if(DEFINED ${variable})
        gint_consumer_quote(quoted_value "${${variable}}")
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/consumer-compiler-check.cmake"
            "if(NOT ${variable} STREQUAL ${quoted_value})\n"
            "    message(FATAL_ERROR \"consumer ${variable} differs from parent toolchain\")\n"
            "endif()\n")
    endif()
endforeach()

get_property(gint_consumer_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(GINT_CONSUMER_BUILD_ARGS
    "-DGINT_CONSUMER_GENERATOR=${CMAKE_GENERATOR}"
    "-DGINT_CONSUMER_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGINT_CONSUMER_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DGINT_CONSUMER_MULTI_CONFIG=${gint_consumer_multi_config}"
    "-DGINT_CONSUMER_CONFIG=$<CONFIG>"
)
