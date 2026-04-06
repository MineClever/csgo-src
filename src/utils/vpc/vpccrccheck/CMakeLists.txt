# vpc/vpccrccheck
# Generated from: vpccrccheck.vpc, vpc_scripts\source_exe_con_base.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_VPC_VPCCRCCHECK_VPCCRCCHECK_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/vpc/vpccrccheck/vpccrccheck.cpp
    ${SRC_DIR}/utils/vpc/vpccrccheck/crccheck_shared.cpp
    ${SRC_DIR}/tier1/checksum_crc.cpp
    ${SRC_DIR}/tier0/tier1_tables.cpp
)

add_executable(utils_vpc_vpccrccheck_vpccrccheck ${UTILS_VPC_VPCCRCCHECK_VPCCRCCHECK_SOURCES})

target_include_directories(utils_vpc_vpccrccheck_vpccrccheck PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_vpc_vpccrccheck_vpccrccheck PRIVATE
    TIER0_STATIC_LIB
)

target_link_libraries(utils_vpc_vpccrccheck_vpccrccheck PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
)

set_target_properties(utils_vpc_vpccrccheck_vpccrccheck PROPERTIES OUTPUT_NAME "vpccrccheck")

valve_publish_exe(utils_vpc_vpccrccheck_vpccrccheck "${GAME_BIN_DIR}")
