# =============================================================================
# interfaces
# Generated from: interfaces.vcxproj
# =============================================================================

set(INTERFACES_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    interfaces.cpp
)

add_library(interfaces STATIC ${INTERFACES_SOURCES})

valve_apply_base_settings(interfaces)

target_compile_definitions(interfaces PRIVATE
    _LIB
    LIBNAME=interfaces
)

valve_set_static_lib_output(interfaces)
