# =============================================================================
# responserules_runtime
# Generated from: responserules_runtime.vcxproj
# =============================================================================

set(RESPONSERULES_RUNTIME_SOURCES
    criteriaset.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    response_system.cpp
    response_types.cpp
    response_types_internal.cpp
    rr_response.cpp
    rr_speechconcept.cpp
    rrrlib.cpp
)

add_library(responserules_runtime STATIC ${RESPONSERULES_RUNTIME_SOURCES})

valve_apply_base_settings(responserules_runtime)

target_include_directories(responserules_runtime PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(responserules_runtime PRIVATE
    _LIB
    LIBNAME=responserules_runtime
)

valve_set_static_lib_output(responserules_runtime)
