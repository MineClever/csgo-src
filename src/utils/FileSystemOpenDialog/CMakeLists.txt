# =============================================================================
# FileSystemOpenDialog
# Generated from: FileSystemOpenDialog.vcxproj
# =============================================================================

set(FILESYSTEMOPENDIALOG_SOURCES
    ${SRC_DIR}/public/tier0/afxmem_override.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    FileDialogApp.cpp
    FileSystemOpenDlg.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    StdAfx.cpp
)

add_library(FileSystemOpenDialog SHARED ${FILESYSTEMOPENDIALOG_SOURCES})

valve_apply_base_settings(FileSystemOpenDialog)

target_include_directories(FileSystemOpenDialog PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(FileSystemOpenDialog PRIVATE
    _USRDLL
    DLLNAME=FileSystemOpenDialog
    MEMOVERRIDE_MODULE=FileSystemOpenDialog
    WINVER=0x502
    NO_WARN_MBCS_MFC_DEPRECATION
)

target_link_libraries(FileSystemOpenDialog PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(FileSystemOpenDialog)

valve_publish_dll(FileSystemOpenDialog "${GAME_BIN_DIR}")
