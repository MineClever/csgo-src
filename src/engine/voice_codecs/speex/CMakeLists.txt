# voice_codecs/speex
# Generated from: vaudio_speex.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(SPEEX_UPSTREAM_DIR "${REPO_ROOT}/thirdparty/speex-upstream")

if(NOT EXISTS "${SPEEX_UPSTREAM_DIR}/win32/config.h")
    message(STATUS "voice_codecs/speex: skipping (missing thirdparty/speex-upstream)")
    return()
endif()

set(ENGINE_VOICE_CODECS_SPEEX_VENDOR_SOURCES
    ${SPEEX_UPSTREAM_DIR}/libspeex/bits.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/cb_search.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_10_16_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_10_32_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_20_32_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_5_256_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_5_64_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/exc_8_128_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/fftwrap.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/filters.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/gain_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/gain_table_lbr.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/hexc_10_32_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/hexc_table.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/high_lsp_tables.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/kiss_fft.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/kiss_fftr.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/lpc.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/lsp.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/lsp_tables_nb.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/ltp.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/modes.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/modes_wb.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/nb_celp.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/quant_lsp.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/sb_celp.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/smallft.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/speex.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/speex_callbacks.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/speex_header.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/stereo.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/vbr.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/vq.c
    ${SPEEX_UPSTREAM_DIR}/libspeex/window.c
)

if(NOT TARGET speex_upstream_libspeex)
    add_library(speex_upstream_libspeex STATIC ${ENGINE_VOICE_CODECS_SPEEX_VENDOR_SOURCES})

    target_include_directories(speex_upstream_libspeex PUBLIC
        ${SPEEX_UPSTREAM_DIR}/include
        ${SPEEX_UPSTREAM_DIR}/include/speex
        ${SPEEX_UPSTREAM_DIR}/libspeex
        ${SPEEX_UPSTREAM_DIR}/win32
    )

    target_compile_definitions(speex_upstream_libspeex PRIVATE
        HAVE_CONFIG_H
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )
endif()

set(ENGINE_VOICE_CODECS_SPEEX_VAUDIO_SPEEX_SOURCES
    ${SRC_DIR}/tier1/interface.cpp
    ${SRC_DIR}/engine/voice_codecs/frame_encoder/voice_codec_frame.cpp
    ${SRC_DIR}/engine/voice_codecs/speex/VoiceEncoder_Speex.cpp
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/public/vaudio/ivaudio.h
    ${SRC_DIR}/engine/voice_codecs/speex/VoiceEncoder_Speex.h
)

add_library(engine_voice_codecs_speex_vaudio_speex SHARED ${ENGINE_VOICE_CODECS_SPEEX_VAUDIO_SPEEX_SOURCES})

valve_apply_base_settings(engine_voice_codecs_speex_vaudio_speex)

target_include_directories(engine_voice_codecs_speex_vaudio_speex PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/engine/voice_codecs/frame_encoder
    ${SRC_DIR}/engine
    ${SRC_DIR}/engine/audio/public
    ${SPEEX_UPSTREAM_DIR}/include
    ${SPEEX_UPSTREAM_DIR}/include/speex
    ${SPEEX_UPSTREAM_DIR}/libspeex
    ${SPEEX_UPSTREAM_DIR}/win32
)

target_compile_definitions(engine_voice_codecs_speex_vaudio_speex PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    SPEEX_EXPORTS
)

target_link_libraries(engine_voice_codecs_speex_vaudio_speex PRIVATE
    tier0
    tier1
    vstdlib
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    speex_upstream_libspeex
)

set_target_properties(engine_voice_codecs_speex_vaudio_speex PROPERTIES OUTPUT_NAME "vaudio_speex")

valve_set_dll_importlib_output(engine_voice_codecs_speex_vaudio_speex)
valve_publish_dll(engine_voice_codecs_speex_vaudio_speex "${GAME_BIN_DIR}")
