# =============================================================================
# Unitlib
# Generated from: unitlib.vcxproj
# =============================================================================

set(UNITLIB_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    unitlib.cpp
)

add_library(Unitlib SHARED ${UNITLIB_SOURCES})

valve_apply_base_settings(Unitlib)

target_compile_definitions(Unitlib PRIVATE
    _USRDLL
    DLLNAME=unitlib
    MEMOVERRIDE_MODULE=unitlib
    UNITLIB_DLL_EXPORT
)

target_link_libraries(Unitlib PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    # 系统库
    odbc32.lib
    odbccp32.lib
)

valve_set_dll_importlib_output(Unitlib)

valve_publish_dll(Unitlib "${GAME_BIN_DIR}")
