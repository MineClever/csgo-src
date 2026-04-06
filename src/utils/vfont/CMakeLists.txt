# utils/vfont
# Generated from: vfont.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_VFONT_VFONT_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/vfont/vfont.cpp
)

add_executable(utils_vfont_vfont ${UTILS_VFONT_VFONT_SOURCES})

valve_apply_base_settings(utils_vfont_vfont)

target_include_directories(utils_vfont_vfont PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
)

target_link_libraries(utils_vfont_vfont PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    winmm.lib
    comctl32.lib
    tier2
    tier3
)

set_target_properties(utils_vfont_vfont PROPERTIES OUTPUT_NAME "vfont")

valve_publish_exe(utils_vfont_vfont "${GAME_BIN_DIR}")

# utils/vfont
# Generated from: vfont_decompiler.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_VFONT_VFONT_DECOMPILER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/vfont/vfont_decompiler.cpp
)

add_executable(utils_vfont_vfont_decompiler ${UTILS_VFONT_VFONT_DECOMPILER_SOURCES})

valve_apply_base_settings(utils_vfont_vfont_decompiler)

target_include_directories(utils_vfont_vfont_decompiler PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
)

target_link_libraries(utils_vfont_vfont_decompiler PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    winmm.lib
    comctl32.lib
    tier2
    tier3
)

set_target_properties(utils_vfont_vfont_decompiler PROPERTIES OUTPUT_NAME "vfont_decompiler")

valve_publish_exe(utils_vfont_vfont_decompiler "${GAME_BIN_DIR}")
