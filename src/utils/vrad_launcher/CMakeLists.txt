# =============================================================================
# Vrad_launcher
# Generated from: vrad_launcher.vcxproj
# =============================================================================

set(VRAD_LAUNCHER_SOURCES
    stdafx.cpp
    vrad_launcher.cpp
)

add_executable(Vrad_launcher ${VRAD_LAUNCHER_SOURCES})

valve_apply_base_settings(Vrad_launcher)

target_include_directories(Vrad_launcher PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Vrad_launcher PRIVATE
    MEMOVERRIDE_MODULE=vrad_launcher
)

target_link_libraries(Vrad_launcher PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Vrad_launcher "${GAME_DIR}")
