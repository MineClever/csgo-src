# =============================================================================
# datacache
# Generated from: datacache.vcxproj
# =============================================================================

set(DATACACHE_SOURCES
    datacache.cpp
    ${SRC_DIR}/utils/common/datalinker.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    mdlcache.cpp
    mdlcombine.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    precachesystem.cpp
    resourceaccesscontrol.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/public/studio_virtualmodel.cpp
    ${SRC_DIR}/common/studiobyteswap.cpp
    vtfcombine.cpp
)

add_library(datacache SHARED ${DATACACHE_SOURCES})

valve_apply_base_settings(datacache)

target_compile_definitions(datacache PRIVATE
    _USRDLL
    DLLNAME=datacache
    MEMOVERRIDE_MODULE=datacache
    MDLCACHE_DLL_EXPORT
)

target_link_libraries(datacache PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    mathlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(datacache)

valve_publish_dll(datacache "${GAME_BIN_DIR}")
