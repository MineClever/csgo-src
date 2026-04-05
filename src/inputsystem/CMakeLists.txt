# =============================================================================
# inputsystem
# Generated from: inputsystem.vcxproj
# =============================================================================

set(INPUTSYSTEM_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    inputstacksystem.cpp
    inputsystem.cpp
    joystick.cpp
    key_translation.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/common/platforminputdevice.cpp
    steamcontroller.cpp
    xcontroller.cpp
)

add_library(inputsystem SHARED ${INPUTSYSTEM_SOURCES})

valve_apply_base_settings(inputsystem)

target_compile_definitions(inputsystem PRIVATE
    _USRDLL
    DLLNAME=inputsystem
    MEMOVERRIDE_MODULE=inputsystem
    NO_STRING_T
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
)

target_link_libraries(inputsystem PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    mathlib
    # 系统库
    Winmm.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    imm32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(inputsystem)

valve_publish_dll(inputsystem "${GAME_BIN_DIR}")
