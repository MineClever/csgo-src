# =============================================================================
# Vvis_launcher
# Generated from: vvis_launcher.vcxproj
# =============================================================================

set(VVIS_LAUNCHER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    StdAfx.cpp
    vvis_launcher.cpp
)

add_executable(Vvis_launcher ${VVIS_LAUNCHER_SOURCES})

valve_apply_base_settings(Vvis_launcher)

target_include_directories(Vvis_launcher PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Vvis_launcher PRIVATE
    MEMOVERRIDE_MODULE=vvis_launcher
)

target_link_libraries(Vvis_launcher PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Vvis_launcher "${GAME_DIR}")
