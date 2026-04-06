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
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    appframework
    prebuilt::Steam
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

# DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR uses WinMain - needs SUBSYSTEM:WINDOWS
set_target_properties(Hammer_launcher PROPERTIES WIN32_EXECUTABLE TRUE)

valve_publish_exe(Hammer_launcher "${GAME_BIN_DIR}")
