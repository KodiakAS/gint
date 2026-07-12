if(NOT CMAKE_CXX_COMPILER_ID)
    message(FATAL_ERROR "gint requires the CXX language to be enabled before it is configured")
endif()

if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "gint does not support MSVC or clang-cl. Use GCC, Clang, or AppleClang in GNU-compatible mode."
    )
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "gint supports GCC, Clang, and AppleClang only; detected '${CMAKE_CXX_COMPILER_ID}'."
    )
endif()
