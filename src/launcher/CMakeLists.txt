# =============================================================================
# launcher
# Generated from: launcher.vcxproj
# =============================================================================

set(LAUNCHER_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    launcher.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    reslistgenerator.cpp
)

add_library(launcher SHARED ${LAUNCHER_SOURCES})

valve_apply_base_settings(launcher)

target_compile_definitions(launcher PRIVATE
    _USRDLL
    DLLNAME=launcher
    MEMOVERRIDE_MODULE=launcher
    LAUNCHERONLY
)

target_link_libraries(launcher PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    shlwapi.lib
    user32.lib
    winmm.lib
    wsock32.lib
    # 其他（路径引用）
    "../dx9sdk/lib/dinput8.lib"
)

valve_set_dll_importlib_output(launcher)

valve_publish_dll(launcher "${GAME_BIN_DIR}")
