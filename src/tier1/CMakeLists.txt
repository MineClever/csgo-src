# =============================================================================
# tier1
# Generated from: tier1.vcxproj
# =============================================================================

set(TIER1_SOURCES
    ${SRC_DIR}/utils/lzma/C/LzmaDec.c
    appinstance.cpp
    bitbuf.cpp
    byteswap.cpp
    characterset.cpp
    checksum_crc.cpp
    checksum_md5.cpp
    checksum_sha1.cpp
    circularbuffer.cpp
    commandbuffer.cpp
    convar.cpp
    datamanager.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    diff.cpp
    exprevaluator.cpp
    generichash.cpp
    interface.cpp
    keyvalues.cpp
    keyvaluesjson.cpp
    kvpacker.cpp
    lzmaDecoder.cpp
    lzss.cpp
    memoverride_dummy.cpp
    mempool.cpp
    memstack.cpp
    miniprofiler_hash.cpp
    NetAdr.cpp
    newbitbuf.cpp
    processor_detect.cpp  # EH
    rangecheckedvar.cpp
    sparsematrix.cpp
    splitstring.cpp
    stringpool.cpp
    strtools.cpp
    strtools_unicode.cpp
    tier1.cpp
    tier1_logging.cpp
    timeutils.cpp
    uniqueid.cpp
    utlbuffer.cpp
    utlbufferutil.cpp
    utlsoacontainer.cpp
    utlstring.cpp
    utlstringtoken.cpp
    utlsymbol.cpp
)

add_library(tier1 STATIC ${TIER1_SOURCES})

valve_apply_base_settings(tier1)

target_compile_definitions(tier1 PRIVATE
    _LIB
    LIBNAME=tier1
    TIER1_STATIC_LIB
)

# 这些文件需要单独开启异常处理
set_source_files_properties(processor_detect.cpp PROPERTIES COMPILE_OPTIONS "/EHsc")

target_link_libraries(tier1 PRIVATE
    # 系统库
    Rpcrt4.lib
)

valve_set_static_lib_output(tier1)
