if(NOT DEFINED GINT_CONSUMER_KIND OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CONSUMER_KIND, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(consumer_source "${GINT_SOURCE_DIR}/tests/consumer/${GINT_CONSUMER_KIND}")
set(consumer_build "${GINT_BINARY_DIR}/consumer-${GINT_CONSUMER_KIND}")

if(GINT_CONSUMER_KIND STREQUAL "package")
    set(install_prefix "${GINT_BINARY_DIR}/consumer-package-prefix")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${GINT_BINARY_DIR}" --prefix "${install_prefix}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "failed to install gint package: ${result}")
    endif()
    set(configure_args -DCMAKE_PREFIX_PATH=${install_prefix})
elseif(GINT_CONSUMER_KIND STREQUAL "subproject")
    set(configure_args -DGINT_SOURCE_DIR=${GINT_SOURCE_DIR})
else()
    message(FATAL_ERROR "unknown consumer kind: ${GINT_CONSUMER_KIND}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${consumer_build}" ${configure_args}
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to configure ${GINT_CONSUMER_KIND} consumer: ${result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to build ${GINT_CONSUMER_KIND} consumer: ${result}")
endif()
