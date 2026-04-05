# =============================================================================
# Commedit
# Generated from: commedit.vcxproj
# =============================================================================

set(COMMEDIT_SOURCES
    commeditdoc.cpp
    commedittool.cpp
    commentarynodebrowserpanel.cpp
    commentarypropertiespanel.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    dmecommentarynodeentity.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/registry.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
)

add_library(Commedit SHARED ${COMMEDIT_SOURCES})

valve_apply_base_settings(Commedit)

target_include_directories(Commedit PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/tools/commedit
    ${SRC_DIR}/game/shared
)

target_compile_definitions(Commedit PRIVATE
    _USRDLL
    DLLNAME=commedit
    MEMOVERRIDE_MODULE=commedit
    COMMEDIT_EXPORTS
)

target_link_libraries(Commedit PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    Dmxloader
    mathlib
    vgui_controls
    # 系统库
    Psapi.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(Commedit)

valve_publish_dll(Commedit "${GAME_BIN_DIR}")
