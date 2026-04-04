# =============================================================================
# Raytrace
# Generated from: raytrace.vcxproj
# =============================================================================

set(RAYTRACE_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    raytrace.cpp
    trace2.cpp
    trace3.cpp
)

add_library(Raytrace STATIC ${RAYTRACE_SOURCES})

valve_apply_base_settings(Raytrace)

target_include_directories(Raytrace PRIVATE
    ${SRC_DIR}/utils/common
)

target_compile_definitions(Raytrace PRIVATE
    _LIB
    LIBNAME=raytrace
)

valve_set_static_lib_output(Raytrace)
