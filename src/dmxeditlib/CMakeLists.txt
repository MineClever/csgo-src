# src/dmxeditlib
# Generated from: dmxeditlib.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(DMXEDITLIB_DMXEDITLIB_SOURCES
    ${SRC_DIR}/dmxeditlib/dmxedit.cpp
    ${SRC_DIR}/dmxeditlib/winstuff.cpp
    ${SRC_DIR}/public/dmxeditlib/dmxedit.h
)

add_library(dmxeditlib_dmxeditlib STATIC ${DMXEDITLIB_DMXEDITLIB_SOURCES})

valve_apply_base_settings(dmxeditlib_dmxeditlib)

target_include_directories(dmxeditlib_dmxeditlib PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(dmxeditlib_dmxeditlib PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DMXEDIT_LIB
)

set_target_properties(dmxeditlib_dmxeditlib PROPERTIES OUTPUT_NAME "dmxeditlib")

valve_set_static_lib_output(dmxeditlib_dmxeditlib)
