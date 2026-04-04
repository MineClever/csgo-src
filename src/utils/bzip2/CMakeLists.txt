# =============================================================================
# Bzip2
# Generated from: bzip2.vcxproj
# =============================================================================

set(BZIP2_SOURCES
    blocksort.c
    bzip2.c
    bzlib.c
    compress.c
    crctable.c
    ${SRC_DIR}/common/debug_lib_check.cpp
    decompress.c
    huffman.c
    randtable.c
)

add_library(Bzip2 STATIC ${BZIP2_SOURCES})

valve_apply_base_settings(Bzip2)

target_include_directories(Bzip2 PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Bzip2 PRIVATE
    _LIB
    LIBNAME=bzip2
)

valve_set_static_lib_output(Bzip2)
