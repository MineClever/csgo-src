# =============================================================================
# SceneFileCache
# Generated from: scenefilecache.vcxproj
# =============================================================================

set(SCENEFILECACHE_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    SceneFileCache.cpp
)

add_library(SceneFileCache SHARED ${SCENEFILECACHE_SOURCES})

valve_apply_base_settings(SceneFileCache)

target_include_directories(SceneFileCache PRIVATE
    ${SRC_DIR}/scenefilecache
    ${SRC_DIR}/game/shared
)

target_compile_definitions(SceneFileCache PRIVATE
    _USRDLL
    DLLNAME=scenefilecache
    MEMOVERRIDE_MODULE=scenefilecache
    PROTECTED_THINGS_ENABLE
)

target_link_libraries(SceneFileCache PRIVATE
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

valve_set_dll_importlib_output(SceneFileCache)

valve_publish_dll(SceneFileCache "${GAME_BIN_DIR}")
