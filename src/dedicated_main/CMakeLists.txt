# =============================================================================
# Dedicated_main
# Generated from: dedicated_main.vcxproj
# =============================================================================

set(DEDICATED_MAIN_SOURCES
    main.cpp
    ${SRC_DIR}/common/SteamAppStartup.cpp
)

add_executable(Dedicated_main ${DEDICATED_MAIN_SOURCES})

valve_apply_base_settings(Dedicated_main)

target_compile_definitions(Dedicated_main PRIVATE
    MEMOVERRIDE_MODULE=dedicated_main
)

target_link_libraries(Dedicated_main PRIVATE
    # 系统库
    Advapi32.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

# WinMain entry point requires SUBSYSTEM:WINDOWS
set_target_properties(Dedicated_main PROPERTIES WIN32_EXECUTABLE TRUE)

valve_publish_exe(Dedicated_main "${GAME_BIN_DIR}")
