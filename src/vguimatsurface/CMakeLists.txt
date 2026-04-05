# =============================================================================
# vguimatsurface
# Generated from: vguimatsurface.vcxproj
# =============================================================================

set(VGUIMATSURFACE_SOURCES
    Clip2D.cpp
    Cursor.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/vgui2/src/htmlwindow.cpp
    Input.cpp
    MatSystemSurface.cpp
    memorybitmap.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    ${SRC_DIR}/vgui2/src/vgui_key_translation.cpp
)

add_library(vguimatsurface SHARED ${VGUIMATSURFACE_SOURCES})

valve_apply_base_settings(vguimatsurface)

target_compile_definitions(vguimatsurface PRIVATE
    _USRDLL
    DLLNAME=vguimatsurface
    MEMOVERRIDE_MODULE=vguimatsurface
    BENCHMARK
    VGUIMATSURFACE_DLL_EXPORT
    GAMEUI_EXPORTS
    DONT_PROTECT_FILEIO_FUNCTIONS
    PROTECTED_THINGS_ENABLE
    fopen=dont_use_fopen
    ENABLE_CHROMEHTMLWINDOW
)

target_link_libraries(vguimatsurface PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    bitmap
    mathlib
    vgui_controls
    vgui_surfacelib
    Dmxloader
    prebuilt::nvtc
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(vguimatsurface)

valve_publish_dll(vguimatsurface "${GAME_BIN_DIR}")
