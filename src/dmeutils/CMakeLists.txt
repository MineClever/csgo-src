# src/dmeutils
# Generated from: dmeutils.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(DMEUTILS_DMEUTILS_SOURCES
    ${SRC_DIR}/dmeutils/dmmeshutils.cpp
    ${SRC_DIR}/dmeutils/dmanimutils.cpp
    ${SRC_DIR}/dmeutils/loadfromdmx.cpp
    ${SRC_DIR}/dmeutils/savetodmx.cpp
    ${SRC_DIR}/public/dmeutils/dmmeshutils.h
    ${SRC_DIR}/public/dmeutils/dmanimutils.h
)

add_library(dmeutils_dmeutils STATIC ${DMEUTILS_DMEUTILS_SOURCES})
add_library(dmeutils ALIAS dmeutils_dmeutils)

valve_apply_base_settings(dmeutils_dmeutils)

target_include_directories(dmeutils_dmeutils PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(dmeutils_dmeutils PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DMEUTILS_LIB
)

set_target_properties(dmeutils_dmeutils PROPERTIES OUTPUT_NAME "dmeutils")

valve_set_static_lib_output(dmeutils_dmeutils)
