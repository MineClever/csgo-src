# utils/libmad
# Generated from: libmad.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_LIBMAD_LIBMAD_SOURCES
    ${SRC_DIR}/utils/libmad/bit.c
    ${SRC_DIR}/utils/libmad/decoder.c
    ${SRC_DIR}/utils/libmad/fixed.c
    ${SRC_DIR}/utils/libmad/frame.c
    ${SRC_DIR}/utils/libmad/huffman.c
    ${SRC_DIR}/utils/libmad/layer12.c
    ${SRC_DIR}/utils/libmad/layer3.c
    ${SRC_DIR}/utils/libmad/stream.c
    ${SRC_DIR}/utils/libmad/synth.c
    ${SRC_DIR}/utils/libmad/timer.c
    ${SRC_DIR}/utils/libmad/version.c
    ${SRC_DIR}/utils/libmad/bit.h
    ${SRC_DIR}/utils/libmad/msvc++/config.h
    ${SRC_DIR}/utils/libmad/decoder.h
    ${SRC_DIR}/utils/libmad/fixed.h
    ${SRC_DIR}/utils/libmad/frame.h
    ${SRC_DIR}/utils/libmad/global.h
    ${SRC_DIR}/utils/libmad/huffman.h
    ${SRC_DIR}/utils/libmad/layer12.h
    ${SRC_DIR}/utils/libmad/layer3.h
    ${SRC_DIR}/utils/libmad/stream.h
    ${SRC_DIR}/utils/libmad/synth.h
    ${SRC_DIR}/utils/libmad/timer.h
    ${SRC_DIR}/utils/libmad/version.h
)

add_library(utils_libmad_libmad STATIC ${UTILS_LIBMAD_LIBMAD_SOURCES})

valve_apply_base_settings(utils_libmad_libmad)

target_include_directories(utils_libmad_libmad PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/libmad/msvc++
)

target_compile_definitions(utils_libmad_libmad PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    FPM_INTEL
    HAVE_CONFIG_H
    ASO_ZEROCHECK
)

set_target_properties(utils_libmad_libmad PROPERTIES OUTPUT_NAME "libmad")

valve_set_static_lib_output(utils_libmad_libmad)
