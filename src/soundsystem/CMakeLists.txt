# =============================================================================
# Soundsystem
# Generated from: soundsystem.vcxproj
# =============================================================================

set(SOUNDSYSTEM_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    ${SRC_DIR}/public/sentence.cpp
    snd_audio_source.cpp
    snd_dev_wave.cpp
    snd_io.cpp
    snd_wave_mixer.cpp
    snd_wave_mixer_adpcm.cpp
    snd_wave_source.cpp
    soundsystem.cpp
)

add_library(Soundsystem SHARED ${SOUNDSYSTEM_SOURCES})

valve_apply_base_settings(Soundsystem)

target_compile_definitions(Soundsystem PRIVATE
    _USRDLL
    DLLNAME=soundsystem
    MEMOVERRIDE_MODULE=soundsystem
    SOUNDSYSTEM_EXPORTS
)

target_link_libraries(Soundsystem PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    mathlib
    soundsystem_lowlevel
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
)

valve_set_dll_importlib_output(Soundsystem)

valve_publish_dll(Soundsystem "${GAME_BIN_DIR}")


add_subdirectory(lowlevel)