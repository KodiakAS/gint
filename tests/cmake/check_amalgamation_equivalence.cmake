if(NOT DEFINED GINT_GRAPH_EXECUTABLE OR NOT DEFINED GINT_FLAT_EXECUTABLE)
    message(FATAL_ERROR "Both configured compiler fixture executables are required")
endif()

foreach(kind GRAPH FLAT)
    execute_process(
        COMMAND "${GINT_${kind}_EXECUTABLE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output_${kind}
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${kind} fixture failed (${result}): ${error}")
    endif()
endforeach()

if(NOT "${output_GRAPH}" STREQUAL "${output_FLAT}")
    message(FATAL_ERROR
        "Source graph and generated header disagree:\n"
        "graph: ${output_GRAPH}\nflat: ${output_FLAT}"
    )
endif()
