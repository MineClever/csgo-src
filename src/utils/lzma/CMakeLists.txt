# utils/lzma
# Generated from: lzma.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_LZMA_LZMA_SOURCES
    ${SRC_DIR}/utils/lzma/lzma.cpp
    ${SRC_DIR}/utils/lzma/C/LzmaEnc.c
    ${SRC_DIR}/utils/lzma/C/LzmaDec.c
    ${SRC_DIR}/utils/lzma/C/LzFind.c
    ${SRC_DIR}/utils/lzma/C/7zTypes.h
    ${SRC_DIR}/utils/lzma/C/LzmaEnc.h
    ${SRC_DIR}/utils/lzma/C/LzmaDec.h
    ${SRC_DIR}/utils/lzma/C/LzFind.h
    ${SRC_DIR}/utils/lzma/C/LzHash.h
)

add_library(utils_lzma_lzma STATIC ${UTILS_LZMA_LZMA_SOURCES})

valve_apply_base_settings(utils_lzma_lzma)

target_include_directories(utils_lzma_lzma PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/lzma/C
)

target_compile_definitions(utils_lzma_lzma PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    _NO_EXCEPTIONS
    _LZMA_PROB32
    _7ZIP_ST
)

set_target_properties(utils_lzma_lzma PROPERTIES OUTPUT_NAME "lzma")

valve_set_static_lib_output(utils_lzma_lzma)
