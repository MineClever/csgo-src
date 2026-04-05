# =============================================================================
# localize
# Generated from: localize.vcxproj
# =============================================================================

set(LOCALIZE_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    localize.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/registry.cpp
    ${SRC_DIR}/public/UnicodeFileHelpers.cpp
)

add_library(localize SHARED ${LOCALIZE_SOURCES})

valve_apply_base_settings(localize)

target_compile_definitions(localize PRIVATE
    _USRDLL
    DLLNAME=localize
    MEMOVERRIDE_MODULE=localize
)

target_link_libraries(localize PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    appframework
    vstdlib
    tier3
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(localize)

valve_publish_dll(localize "${GAME_BIN_DIR}")
