# voice_codecs/miles
# Generated from: vaudio_miles.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(MILES_SDK_DIR "${SRC_DIR}/thirdparty/MilesSDKWin")
set(MILES_SDK_INCLUDE_DIR "${MILES_SDK_DIR}/include")
set(MILES_SDK_REDIST_DIR "${MILES_SDK_DIR}/redist")

if(VALVE_TARGET_ARCH STREQUAL "x64")
    set(MILES_SDK_LIB "${MILES_SDK_DIR}/lib/mss64.lib")
else()
    set(MILES_SDK_LIB "${MILES_SDK_DIR}/lib/mss32.lib")
endif()

if(NOT EXISTS "${MILES_SDK_INCLUDE_DIR}/mss.h" OR NOT EXISTS "${MILES_SDK_LIB}")
    message(STATUS "voice_codecs/miles: skipping (missing Miles SDK under ${MILES_SDK_DIR})")
    return()
endif()

set(ENGINE_VOICE_CODECS_MILES_VAUDIO_MILES_SOURCES
    ${SRC_DIR}/engine/voice_codecs/miles/milesbase.cpp
    ${SRC_DIR}/engine/voice_codecs/miles/mp3_miles.cpp
    ${SRC_DIR}/engine/voice_codecs/frame_encoder/voice_codec_frame.cpp
    ${SRC_DIR}/engine/voice_codecs/miles/voice_codec_miles.cpp
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/public/vaudio/ivaudio.h
    ${SRC_DIR}/engine/voice_codecs/miles/milesbase.h
)

add_library(engine_voice_codecs_miles_VAudio_Miles SHARED ${ENGINE_VOICE_CODECS_MILES_VAUDIO_MILES_SOURCES})

valve_apply_base_settings(engine_voice_codecs_miles_VAudio_Miles)

target_include_directories(engine_voice_codecs_miles_VAudio_Miles PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/engine/voice_codecs/frame_encoder
    ${SRC_DIR}/engine
    ${MILES_SDK_INCLUDE_DIR}
)

target_compile_definitions(engine_voice_codecs_miles_VAudio_Miles PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
)

target_link_libraries(engine_voice_codecs_miles_VAudio_Miles PRIVATE
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
    "${MILES_SDK_LIB}"
)

set_target_properties(engine_voice_codecs_miles_VAudio_Miles PROPERTIES OUTPUT_NAME "VAudio_Miles")

valve_set_dll_importlib_output(engine_voice_codecs_miles_VAudio_Miles)
valve_publish_dll(engine_voice_codecs_miles_VAudio_Miles "${GAME_BIN_DIR}")

if(EXISTS "${MILES_SDK_REDIST_DIR}")
    file(GLOB MILES_SDK_REDIST_FILES "${MILES_SDK_REDIST_DIR}/*")
    foreach(_miles_redist_file IN LISTS MILES_SDK_REDIST_FILES)
        add_custom_command(TARGET engine_voice_codecs_miles_VAudio_Miles POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_miles_redist_file}" "${GAME_BIN_DIR}"
        )
    endforeach()
endif()
