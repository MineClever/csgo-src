# =============================================================================
# launcher
# Generated from: launcher.vcxproj
# =============================================================================

set(LAUNCHER_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    launcher.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    reslistgenerator.cpp
)

add_library(launcher SHARED ${LAUNCHER_SOURCES})

valve_apply_base_settings(launcher)

target_compile_definitions(launcher PRIVATE
    _USRDLL
    DLLNAME=launcher
    MEMOVERRIDE_MODULE=launcher
    LAUNCHERONLY
)

target_link_libraries(launcher PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    appframework
    prebuilt::steam_api
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    shlwapi.lib
    user32.lib
    winmm.lib
    wsock32.lib
    # DirectX
    prebuilt::dinput8
)

valve_set_dll_importlib_output(launcher)

valve_publish_dll(launcher "${GAME_BIN_DIR}")
