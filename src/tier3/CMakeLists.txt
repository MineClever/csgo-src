# =============================================================================
# tier3
# Generated from: tier3.vcxproj
# =============================================================================

set(TIER3_SOURCES
    choreoutils.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    mdlutils.cpp
    scenetokenprocessor.cpp
    studiohdrstub.cpp
    tier3.cpp
)

add_library(tier3 STATIC ${TIER3_SOURCES})

valve_apply_base_settings(tier3)

target_compile_definitions(tier3 PRIVATE
    _LIB
    LIBNAME=tier3
)

valve_set_static_lib_output(tier3)
