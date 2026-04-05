# =============================================================================
# Dedicated
# Generated from: dedicated.vcxproj
# =============================================================================

set(DEDICATED_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    filesystem.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/common/netapi.cpp
    ${SRC_DIR}/common/SteamAppStartup.cpp
    sys_common.cpp
    sys_ded.cpp
    sys_subproc.cpp
    sys_windows.cpp
    ${SRC_DIR}/dedicated/console/conproc.cpp
    ${SRC_DIR}/dedicated/console/textconsole.cpp
    ${SRC_DIR}/dedicated/console/TextConsoleWin32.cpp
    ${SRC_DIR}/filesystem/basefilesystem.cpp
    ${SRC_DIR}/filesystem/filesystem_async.cpp
    ${SRC_DIR}/filesystem/filesystem_stdio.cpp
    ${SRC_DIR}/filesystem/filetracker.cpp
    ${SRC_DIR}/filesystem/packfile.cpp
    ${SRC_DIR}/filesystem/QueuedLoader.cpp
    ${SRC_DIR}/public/zip_utils.cpp
    ${SRC_DIR}/dedicated/vgui/CreateMultiplayerGameServerPage.cpp
    ${SRC_DIR}/dedicated/vgui/MainPanel.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    ${SRC_DIR}/dedicated/vgui/vguihelpers.cpp
)

add_library(Dedicated SHARED ${DEDICATED_SOURCES})

valve_apply_base_settings(Dedicated)

target_include_directories(Dedicated PRIVATE
    ${SRC_DIR}/dedicated
    ${SRC_DIR}/engine
)

target_compile_definitions(Dedicated PRIVATE
    _USRDLL
    DLLNAME=dedicated
    MEMOVERRIDE_MODULE=dedicated
    DEDICATED
    LAUNCHERONLY
)

target_link_libraries(Dedicated PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    appframework
    Dmxloader
    mathlib
    vpklib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
    wsock32.lib
)

valve_set_dll_importlib_output(Dedicated)

valve_publish_dll(Dedicated "${GAME_BIN_DIR}")
