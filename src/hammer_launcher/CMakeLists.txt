# =============================================================================
# Hammer_launcher
# Generated from: hammer_launcher.vcxproj
# =============================================================================

set(HAMMER_LAUNCHER_SOURCES
    main.cpp
)

add_executable(Hammer_launcher ${HAMMER_LAUNCHER_SOURCES})

valve_apply_base_settings(Hammer_launcher)

target_compile_definitions(Hammer_launcher PRIVATE
    MEMOVERRIDE_MODULE=hammer_launcher
)

target_link_libraries(Hammer_launcher PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Hammer_launcher "${GAME_DIR}")
