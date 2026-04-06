# =============================================================================
# Vtex_dll
# Generated from: vtex_dll.vcxproj
# =============================================================================

set(VTEX_DLL_SOURCES
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/public/materialobjects/materialobjects.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    vtex.cpp
)

add_library(Vtex_dll SHARED ${VTEX_DLL_SOURCES})

valve_apply_base_settings(Vtex_dll)

target_include_directories(Vtex_dll PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
)

target_compile_definitions(Vtex_dll PRIVATE
    _USRDLL
    DLLNAME=vtex_dll
    MEMOVERRIDE_MODULE=vtex_dll
    VTEX_DLL
    VTEX_DLL_EXPORTS
    PROTECTED_THINGS_DISABLE
)

target_link_libraries(Vtex_dll PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    mathlib
    bitmap
    vtf
    Datamodel
    Materialobjects
    Dmserializers
    resourcefile
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(Vtex_dll)

valve_publish_dll(Vtex_dll "${GAME_BIN_DIR}")

# =============================================================================
# Vtex_launcher
# Generated from: vtex_launcher.vcxproj
# =============================================================================

set(VTEX_LAUNCHER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    vtex_launcher.cpp
)

add_executable(Vtex_launcher ${VTEX_LAUNCHER_SOURCES})

valve_apply_base_settings(Vtex_launcher)

target_include_directories(Vtex_launcher PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Vtex_launcher PRIVATE
    MEMOVERRIDE_MODULE=vtex_launcher
    PROTECTED_THINGS_DISABLE
)

target_link_libraries(Vtex_launcher PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    appframework
    mathlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Vtex_launcher "${GAME_BIN_DIR}")
