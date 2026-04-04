# =============================================================================
# vpklib
# Generated from: vpklib.vcxproj
# =============================================================================

set(VPKLIB_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    packedstore.cpp
)

add_library(vpklib STATIC ${VPKLIB_SOURCES})

valve_apply_base_settings(vpklib)

target_compile_definitions(vpklib PRIVATE
    _LIB
    LIBNAME=vpklib
)

valve_set_static_lib_output(vpklib)
