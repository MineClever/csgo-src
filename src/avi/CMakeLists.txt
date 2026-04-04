# =============================================================================
# valve_avi
# Generated from: valve_avi.vcxproj
# =============================================================================

set(VALVE_AVI_SOURCES
    avi.cpp
    bink.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
)

add_library(valve_avi SHARED ${VALVE_AVI_SOURCES})

valve_apply_base_settings(valve_avi)

target_include_directories(valve_avi PRIVATE
    ${SRC_DIR}/dx9sdk/include
)

target_compile_definitions(valve_avi PRIVATE
    _USRDLL
    DLLNAME=valve_avi
    MEMOVERRIDE_MODULE=valve_avi
)

target_link_libraries(valve_avi PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    vfw32.lib
)

valve_set_dll_importlib_output(valve_avi)

valve_publish_dll(valve_avi "${GAME_BIN_DIR}")
