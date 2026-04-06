# tracker/AdminServer
# Generated from: AdminServer.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(TRACKER_ADMINSERVER_ADMINSERVER_SOURCES
    ${SRC_DIR}/tracker/AdminServer/AdminServer.cpp
    ${SRC_DIR}/tracker/AdminServer/BanContextMenu.cpp
    ${SRC_DIR}/tracker/AdminServer/ConfigPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/GamePanelInfo.cpp
    ${SRC_DIR}/tracker/AdminServer/MapCycleEditDialog.cpp
    ${SRC_DIR}/tracker/common/msgbuffer.cpp
    ${SRC_DIR}/tracker/AdminServer/PlayerContextMenu.cpp
    ${SRC_DIR}/tracker/AdminServer/PlayerListCompare.cpp
    ${SRC_DIR}/tracker/AdminServer/RemoteServer.cpp
    ${SRC_DIR}/tracker/AdminServer/VarEditDialog.cpp
    ${SRC_DIR}/tracker/AdminServer/VarListPropertyPage.cpp
    ${SRC_DIR}/common/vgui/vgui_basebudgetpanel.cpp
    ${SRC_DIR}/common/vgui/vgui_budgetbargraphpanel.cpp
    ${SRC_DIR}/common/vgui/vgui_budgethistorypanel.cpp
    ${SRC_DIR}/common/vgui/vgui_budgetpanelshared.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    ${SRC_DIR}/tracker/AdminServer/TokenLine.cpp
    ${SRC_DIR}/tracker/AdminServer/DialogAddBan.cpp
    ${SRC_DIR}/tracker/AdminServer/DialogCvarChange.cpp
    ${SRC_DIR}/tracker/AdminServer/BanPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/BudgetPanelContainer.cpp
    ${SRC_DIR}/tracker/AdminServer/ChatPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/GraphPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/PlayerPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/RawLogPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/ServerConfigPanel.cpp
    ${SRC_DIR}/tracker/AdminServer/serverinfopanel.cpp
    ${SRC_DIR}/tracker/AdminServer/AdminServer.h
    ${SRC_DIR}/tracker/AdminServer/BanContextMenu.h
    ${SRC_DIR}/tracker/AdminServer/BanPanel.h
    ${SRC_DIR}/tracker/AdminServer/BudgetPanelContainer.h
    ${SRC_DIR}/tracker/AdminServer/ChatPanel.h
    ${SRC_DIR}/tracker/AdminServer/ConfigPanel.h
    ${SRC_DIR}/tracker/AdminServer/DialogAddBan.h
    ${SRC_DIR}/tracker/AdminServer/DialogCvarChange.h
    ${SRC_DIR}/tracker/AdminServer/GamePanelInfo.h
    ${SRC_DIR}/tracker/AdminServer/GraphPanel.h
    ${SRC_DIR}/common/IGameServerData.h
    ${SRC_DIR}/tracker/AdminServer/IManageServer.h
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/tracker/common/IServerRefreshResponse.h
    ${SRC_DIR}/common/ivprofexport.h
    ${SRC_DIR}/tracker/AdminServer/MapCycleEditDialog.h
    ${SRC_DIR}/tracker/common/msgbuffer.h
    ${SRC_DIR}/tracker/AdminServer/PlayerContextMenu.h
    ${SRC_DIR}/tracker/AdminServer/PlayerListCompare.h
    ${SRC_DIR}/tracker/AdminServer/PlayerPanel.h
    ${SRC_DIR}/tracker/AdminServer/RawLogPanel.h
    ${SRC_DIR}/tracker/AdminServer/RemoteServer.h
    ${SRC_DIR}/tracker/AdminServer/ServerConfigPanel.h
    ${SRC_DIR}/tracker/AdminServer/serverinfopanel.h
    ${SRC_DIR}/tracker/AdminServer/TokenLine.h
    ${SRC_DIR}/public/tier1/utlbuffer.h
    ${SRC_DIR}/public/tier1/utllinkedlist.h
    ${SRC_DIR}/public/tier1/utlsymbol.h
    ${SRC_DIR}/public/tier1/utlvector.h
    ${SRC_DIR}/tracker/AdminServer/VarEditDialog.h
    ${SRC_DIR}/tracker/AdminServer/VarListPropertyPage.h
    ${SRC_DIR}/common/vgui/vgui_basebudgetpanel.h
    ${SRC_DIR}/common/vgui/vgui_budgetbargraphpanel.h
    ${SRC_DIR}/common/vgui/vgui_budgethistorypanel.h
    ${SRC_DIR}/common/vgui/vgui_budgetpanelshared.h
)

add_library(tracker_AdminServer_AdminServer SHARED ${TRACKER_ADMINSERVER_ADMINSERVER_SOURCES})

valve_apply_base_settings(tracker_AdminServer_AdminServer)

target_include_directories(tracker_AdminServer_AdminServer PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(tracker_AdminServer_AdminServer PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    ADMINSERVER_EXPORTS
    BUDGET_ADMIN_SERVER
)

target_link_libraries(tracker_AdminServer_AdminServer PRIVATE
    tier1
    interfaces
    tier2
    tier3
    vgui_controls
    mathlib
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    wsock32.lib
)

set_target_properties(tracker_AdminServer_AdminServer PROPERTIES OUTPUT_NAME "AdminServer")

valve_set_dll_importlib_output(tracker_AdminServer_AdminServer)
valve_publish_dll(tracker_AdminServer_AdminServer "${GAME_BIN_DIR}")
