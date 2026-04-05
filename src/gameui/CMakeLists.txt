# =============================================================================
# GameUI
# Generated from: GameUI.vcxproj
# =============================================================================

set(GAMEUI_SOURCES
    BitmapImagePanel.cpp
    CommandCheckButton.cpp
    CvarNegateCheckButton.cpp
    ${SRC_DIR}/common/GameUI/cvarslider.cpp
    CvarTextEntry.cpp
    CvarToggleCheckButton.cpp
    HapticControlBox.cpp
    KeyToggleCheckButton.cpp
    LabeledCommandComboBox.cpp
    URLButton.cpp
    vcontrolslistpanel.cpp
    BaseSaveGameDialog.cpp
    BenchmarkDialog.cpp
    BonusMapsDialog.cpp
    ChangeGameDialog.cpp
    CommentaryDialog.cpp
    CommentaryExplanationDialog.cpp
    ContentControlDialog.cpp
    CreateMultiplayerGameBotPage.cpp
    CreateMultiplayerGameDialog.cpp
    CreateMultiplayerGameGameplayPage.cpp
    CreateMultiplayerGameServerPage.cpp
    CustomTabExplanationDialog.cpp
    GameConsoleDialog.cpp
    LoadCommentaryDialog.cpp
    LoadGameDialog.cpp
    LoadingDialog.cpp
    MultiplayerAdvancedDialog.cpp
    NewGameDialog.cpp
    PlayerListDialog.cpp
    SaveGameDialog.cpp
    OptionsDialog.cpp
    OptionsSubAudio.cpp
    OptionsSubDifficulty.cpp
    OptionsSubGame.cpp
    OptionsSubHaptics.cpp
    OptionsSubKeyboard.cpp
    OptionsSubMouse.cpp
    OptionsSubMultiplayer.cpp
    OptionsSubPortal.cpp
    OptionsSubVideo.cpp
    OptionsSubVoice.cpp
    BackgroundMenuButton.cpp
    BasePanel.cpp
    BonusMapsDatabase.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    GameConsole.cpp
    GameUI_Interface.cpp
    gameui_util.cpp
    ${SRC_DIR}/common/imageutils.cpp
    ${SRC_DIR}/common/language.cpp
    LogoFile.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ModInfo.cpp
    MouseMessageForwardingPanel.cpp
    ${SRC_DIR}/tracker/common/msgbuffer.cpp
    ${SRC_DIR}/tracker/common/netapi.cpp
    ${SRC_DIR}/common/GameUI/ObjectList.cpp
    PanelListPanel.cpp
    RunGameEngine.cpp
    SaveGameBrowserDialog.cpp
    ${SRC_DIR}/common/GameUI/scriptobject.cpp
    ${SRC_DIR}/tracker/common/Socket.cpp
    Sys_Utils.cpp
    TextEntryBox.cpp
    TGAImagePanel.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    VGuiSystemModuleLoader.cpp
)

add_library(GameUI SHARED ${GAMEUI_SOURCES})

valve_apply_base_settings(GameUI)

target_include_directories(GameUI PRIVATE
    ${SRC_DIR}/gameui
    ${SRC_DIR}/vgui2/include
    ${SRC_DIR}/vgui2/controls
    ${SRC_DIR}/common/GameUI
)

target_compile_definitions(GameUI PRIVATE
    _USRDLL
    DLLNAME=GameUI
    MEMOVERRIDE_MODULE=GameUI
    GAMEUI_EXPORTS
    VERSION_SAFE_STEAM_API_INTERFACES
)

target_link_libraries(GameUI PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    bitmap
    mathlib
    matsys_controls
    vgui_controls
    vtf
    vpklib
    videocfg
    prebuilt::libjpeg
    prebuilt::libpng
    prebuilt::libz
    prebuilt::steam_api
    # 系统库
    Shlwapi.lib
    Ws2_32.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(GameUI)

valve_publish_dll(GameUI "${GAME_BIN_DIR}")
