# =============================================================================
# FileSystem_Stdio
# Generated from: filesystem_stdio.vcxproj
# =============================================================================

set(FILESYSTEM_STDIO_SOURCES
    basefilesystem.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    filesystem_async.cpp
    filesystem_stdio.cpp
    filetracker.cpp
    ${SRC_DIR}/public/kevvaluescompiler.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    packfile.cpp
    QueuedLoader.cpp
    ${SRC_DIR}/public/zip_utils.cpp
)

add_library(FileSystem_Stdio SHARED ${FILESYSTEM_STDIO_SOURCES})

valve_apply_base_settings(FileSystem_Stdio)

target_compile_definitions(FileSystem_Stdio PRIVATE
    _USRDLL
    DLLNAME=filesystem_stdio
    MEMOVERRIDE_MODULE=filesystem_stdio
    FILESYSTEM_STDIO_EXPORTS
    DONT_PROTECT_FILEIO_FUNCTIONS
    PROTECTED_THINGS_ENABLE
    _USE_32BIT_TIME_T
    FILESYSTEM_MSVC2015_STAT_BUG_WORKAROUND
    SUPPORT_PACKED_STORE
)

target_link_libraries(FileSystem_Stdio PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    vpklib
    Bzip2
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(FileSystem_Stdio)

valve_publish_dll(FileSystem_Stdio "${GAME_BIN_DIR}")
