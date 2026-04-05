# =============================================================================
# mdllib
# Generated from: mdllib.vcxproj
# =============================================================================

set(MDLLIB_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    mdllib.cpp
    mdllib_stripinfo.cpp
    mdllib_stripmodel.cpp
    mdllib_utils.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
)

add_library(mdllib SHARED ${MDLLIB_SOURCES})

valve_apply_base_settings(mdllib)

target_compile_definitions(mdllib PRIVATE
    _USRDLL
    DLLNAME=mdllib
    MEMOVERRIDE_MODULE=mdllib
)

target_link_libraries(mdllib PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(mdllib)

valve_publish_dll(mdllib "${GAME_BIN_DIR}")
