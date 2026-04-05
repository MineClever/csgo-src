# =============================================================================
# shaderapiempty
# Generated from: shaderapiempty.vcxproj
# =============================================================================

set(SHADERAPIEMPTY_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    shaderapiempty.cpp
)

add_library(shaderapiempty SHARED ${SHADERAPIEMPTY_SOURCES})

valve_apply_base_settings(shaderapiempty)

target_include_directories(shaderapiempty PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/materialsystem
)

target_compile_definitions(shaderapiempty PRIVATE
    _USRDLL
    DLLNAME=shaderapiempty
    MEMOVERRIDE_MODULE=shaderapiempty
    SHADER_DLL_EXPORT
    PROTECTED_THINGS_ENABLE
)

target_link_libraries(shaderapiempty PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    mathlib
    # 系统库
    odbc32.lib
    odbccp32.lib
)

valve_set_dll_importlib_output(shaderapiempty)

valve_publish_dll(shaderapiempty "${GAME_BIN_DIR}")
