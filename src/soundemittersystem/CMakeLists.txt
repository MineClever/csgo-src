# =============================================================================
# soundemittersystem
# Generated from: soundemittersystem.vcxproj
# =============================================================================

set(SOUNDEMITTERSYSTEM_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    soundemittersystembase.cpp
    ${SRC_DIR}/public/SoundParametersInternal.cpp
)

add_library(soundemittersystem SHARED ${SOUNDEMITTERSYSTEM_SOURCES})

valve_apply_base_settings(soundemittersystem)

target_include_directories(soundemittersystem PRIVATE
    ${SRC_DIR}/soundemittersystem
    ${SRC_DIR}/game/shared
)

target_compile_definitions(soundemittersystem PRIVATE
    _USRDLL
    DLLNAME=soundemittersystem
    MEMOVERRIDE_MODULE=soundemittersystem
    SOUNDEMITTERSYSTEM_EXPORTS
    PROTECTED_THINGS_ENABLE
)

target_link_libraries(soundemittersystem PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    vstdlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(soundemittersystem)

valve_publish_dll(soundemittersystem "${GAME_BIN_DIR}")
