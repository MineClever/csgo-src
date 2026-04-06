# =============================================================================
# Vmt
# Generated from: vmt.vcxproj
# =============================================================================

set(VMT_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/movieobjects/movieobjects.cpp
    ${SRC_DIR}/public/registry.cpp
    ${SRC_DIR}/public/sentence.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    vmtdoc.cpp
    vmttool.cpp
)

add_library(Vmt SHARED ${VMT_SOURCES})

valve_apply_base_settings(Vmt)

target_include_directories(Vmt PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/tools/common
)

target_compile_definitions(Vmt PRIVATE
    _USRDLL
    DLLNAME=vmt
    MEMOVERRIDE_MODULE=vmt
)

target_link_libraries(Vmt PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    Dmxloader
    Datamodel
    Movieobjects
    Dmserializers
    Dme_controls
    bonesetup
    mathlib
    vgui_controls
    matsys_controls
    Toolutils
    prebuilt::dmeutils
    particles
    bitmap
    # 系统库
    Psapi.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(Vmt)

valve_publish_dll(Vmt "${GAME_BIN_DIR}")
