# =============================================================================
# Shadercompile_launcher
# Generated from: shadercompile_launcher.vcxproj
# =============================================================================

set(SHADERCOMPILE_LAUNCHER_SOURCES
    shadercompile_launcher.cpp
)

add_executable(Shadercompile_launcher ${SHADERCOMPILE_LAUNCHER_SOURCES})

valve_apply_base_settings(Shadercompile_launcher)

target_include_directories(Shadercompile_launcher PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Shadercompile_launcher PRIVATE
    MEMOVERRIDE_MODULE=shadercompile_launcher
)

target_link_libraries(Shadercompile_launcher PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Shadercompile_launcher "${GAME_DIR}")
