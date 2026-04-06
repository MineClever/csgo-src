# utils/nvtristriplib
# Generated from: nvtristriplib.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_NVTRISTRIPLIB_NVTRISTRIPLIB_SOURCES
    ${SRC_DIR}/utils/nvtristriplib/nvtristrip.cpp
    ${SRC_DIR}/utils/nvtristriplib/nvtristripobjects.cpp
    ${SRC_DIR}/utils/nvtristriplib/nvtristrip.h
    ${SRC_DIR}/utils/nvtristriplib/nvtristripobjects.h
    ${SRC_DIR}/utils/nvtristriplib/vertexcache.h
)

add_library(utils_nvtristriplib_NVTriStripLib STATIC ${UTILS_NVTRISTRIPLIB_NVTRISTRIPLIB_SOURCES})

valve_apply_base_settings(utils_nvtristriplib_NVTriStripLib)

target_include_directories(utils_nvtristriplib_NVTriStripLib PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_nvtristriplib_NVTriStripLib PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
)

set_target_properties(utils_nvtristriplib_NVTriStripLib PROPERTIES OUTPUT_NAME "NVTriStripLib")

valve_set_static_lib_output(utils_nvtristriplib_NVTriStripLib)
