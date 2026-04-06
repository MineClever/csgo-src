# voice_codecs/celt
# Generated from: vaudio_celt.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/libcelt")
    message(STATUS "voice_codecs/celt: skipping (missing bundled source/libcelt)")
    return()
endif()

set(ENGINE_VOICE_CODECS_CELT_VAUDIO_CELT_SOURCES
    ${SRC_DIR}/tier1/interface.cpp
    ${SRC_DIR}/engine/voice_codecs/frame_encoder/voice_codec_frame.cpp
    ${SRC_DIR}/engine/voice_codecs/celt/voiceencoder_celt.cpp
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/public/vaudio/ivaudio.h
)

add_library(engine_voice_codecs_celt_vaudio_celt SHARED ${ENGINE_VOICE_CODECS_CELT_VAUDIO_CELT_SOURCES})

valve_apply_base_settings(engine_voice_codecs_celt_vaudio_celt)

target_include_directories(engine_voice_codecs_celt_vaudio_celt PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/engine/voice_codecs/frame_encoder
    ${SRC_DIR}/engine/voice_codecs/celt
    ${SRC_DIR}/engine
    ${SRC_DIR}/engine/voice_codecs/celt/source/libcelt
    ${SRC_DIR}/engine/audio/public
)

target_compile_definitions(engine_voice_codecs_celt_vaudio_celt PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    CELT_EXPORTS
)

target_link_libraries(engine_voice_codecs_celt_vaudio_celt PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    "source/libcelt$PLATSUBDIR/libcelt.lib"
)

set_target_properties(engine_voice_codecs_celt_vaudio_celt PROPERTIES OUTPUT_NAME "vaudio_celt")

valve_set_dll_importlib_output(engine_voice_codecs_celt_vaudio_celt)
valve_publish_dll(engine_voice_codecs_celt_vaudio_celt "${GAME_BIN_DIR}")
