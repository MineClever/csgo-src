# =============================================================================
# vgui2
# Generated from: vgui_dll.vcxproj
# =============================================================================

set(VGUI2_SOURCES
    Bitmap.cpp
    Border.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    fileimage.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    htmlwindow.cpp
    ImageBorder.cpp
    InputWin32.cpp
    keyrepeat.cpp
    MemoryBitmap.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    MessageListener.cpp
    ScalableImageBorder.cpp
    Scheme.cpp
    Surface.cpp
    System.cpp
    ${SRC_DIR}/public/UnicodeFileHelpers.cpp
    vgui.cpp
    vgui_internal.cpp
    vgui_key_translation.cpp
    VPanel.cpp
    VPanelWrapper.cpp
)

add_library(vgui2 SHARED ${VGUI2_SOURCES})

valve_apply_base_settings(vgui2)

target_include_directories(vgui2 PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vgui2/include
    ${SRC_DIR}/thirdparty
)

target_compile_definitions(vgui2 PRIVATE
    _USRDLL
    DLLNAME=vgui2
    MEMOVERRIDE_MODULE=vgui_dll
    DONT_PROTECT_FILEIO_FUNCTIONS
)

target_link_libraries(vgui2 PRIVATE
    # 系统库
    Imm32.lib
    Shlwapi.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
)

valve_set_dll_importlib_output(vgui2)

valve_publish_dll(vgui2 "${GAME_BIN_DIR}")
