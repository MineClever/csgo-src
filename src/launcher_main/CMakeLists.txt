# =============================================================================
# launcher_main
# Generated from: launcher_main.vcxproj
# =============================================================================

set(LAUNCHER_MAIN_SOURCES
    main.cpp
)

add_executable(launcher_main ${LAUNCHER_MAIN_SOURCES})

valve_apply_base_settings(launcher_main)

target_compile_definitions(launcher_main PRIVATE
    MEMOVERRIDE_MODULE=launcher_main
)

target_link_libraries(launcher_main PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

# WinMain entry point requires SUBSYSTEM:WINDOWS
set_target_properties(launcher_main PROPERTIES WIN32_EXECUTABLE TRUE)

valve_publish_exe(launcher_main "${GAME_BIN_DIR}")
