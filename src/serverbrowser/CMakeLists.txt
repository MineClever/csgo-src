# =============================================================================
# ServerBrowser
# Generated from: serverbrowser.vcxproj
# =============================================================================

set(SERVERBROWSER_SOURCES
    BaseGamesPage.cpp
    blacklistedservers.cpp
    CustomGames.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    DialogAddServer.cpp
    DialogGameInfo.cpp
    DialogServerPassword.cpp
    FavoriteGames.cpp
    FriendsGames.cpp
    HistoryGames.cpp
    InternetGames.cpp
    LanGames.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ModList.cpp
    quicklistpanel.cpp
    ServerBrowser.cpp
    ServerBrowserDialog.cpp
    ServerContextMenu.cpp
    ServerListCompare.cpp
    SpectateGames.cpp
    VACBannedConnRefusedDialog.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
)

add_library(ServerBrowser SHARED ${SERVERBROWSER_SOURCES})

valve_apply_base_settings(ServerBrowser)

target_compile_definitions(ServerBrowser PRIVATE
    _USRDLL
    DLLNAME=serverbrowser
    MEMOVERRIDE_MODULE=serverbrowser
    VERSION_SAFE_STEAM_API_INTERFACES
    SERVERBROWSER_EXPORTS
    GAME_SRC
    _USE_32BIT_TIME_T
)

target_link_libraries(ServerBrowser PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    vgui_controls
    prebuilt::steam_api
    # 系统库
    Advapi32.lib
    User32.lib
    Ws2_32.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    wsock32.lib
)

valve_set_dll_importlib_output(ServerBrowser)

valve_publish_dll(ServerBrowser "${GAME_BIN_DIR}")
