# utils/binlaunch
# Generated from: binlaunch.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_BINLAUNCH_BINLAUNCH_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/binlaunch/binlaunch.cpp
)

add_executable(utils_binlaunch_binlaunch ${UTILS_BINLAUNCH_BINLAUNCH_SOURCES})

valve_apply_base_settings(utils_binlaunch_binlaunch)

target_include_directories(utils_binlaunch_binlaunch PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_link_libraries(utils_binlaunch_binlaunch PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
)

set_target_properties(utils_binlaunch_binlaunch PROPERTIES OUTPUT_NAME "binlaunch")

valve_publish_exe(utils_binlaunch_binlaunch "${GAME_BIN_DIR}")
