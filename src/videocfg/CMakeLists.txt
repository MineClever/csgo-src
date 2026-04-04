# =============================================================================
# videocfg
# Generated from: videocfg.vcxproj
# =============================================================================

set(VIDEOCFG_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    videocfg.cpp
)

add_library(videocfg STATIC ${VIDEOCFG_SOURCES})

valve_apply_base_settings(videocfg)

target_include_directories(videocfg PRIVATE
    ${SRC_DIR}/public/videocfg
)

target_compile_definitions(videocfg PRIVATE
    _LIB
    LIBNAME=videocfg
)

valve_set_static_lib_output(videocfg)
