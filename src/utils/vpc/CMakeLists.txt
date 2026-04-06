# utils/vpc
# Generated from: vpc.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_VPC_VPC_SOURCES
    ${SRC_DIR}/utils/vpc/baseprojectdatacollector.cpp
    ${SRC_DIR}/utils/vpc/configuration.cpp
    ${SRC_DIR}/utils/vpccrccheck/crccheck_shared.cpp
    ${SRC_DIR}/utils/vpc/dependencies.cpp
    ${SRC_DIR}/utils/vpc/groupscript.cpp
    ${SRC_DIR}/utils/vpc/main.cpp
    ${SRC_DIR}/utils/vpc/p4sln.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_vcproj.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_xcode.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_win32_2010.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_ps3.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_xbox360.cpp
    ${SRC_DIR}/utils/vpc/projectgenerator_xbox360_2010.cpp
    ${SRC_DIR}/utils/vpc/projectscript.cpp
    ${SRC_DIR}/utils/vpc/scriptsource.cpp
    ${SRC_DIR}/utils/vpc/solutiongenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/solutiongenerator_xcode.cpp
    ${SRC_DIR}/utils/vpc/solutiongenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/sys_utils.cpp
    ${SRC_DIR}/utils/vpc/macros.cpp
    ${SRC_DIR}/utils/vpc/conditionals.cpp
    ${SRC_DIR}/utils/vpc/generatordefinition.cpp
    ${SRC_DIR}/utils/vpc/baseprojectdatacollector.h
    ${SRC_DIR}/utils/vpc/dependencies.h
    ${SRC_DIR}/utils/vpc/ibaseprojectgenerator.h
    ${SRC_DIR}/utils/vpc/ibasesolutiongenerator.h
    ${SRC_DIR}/utils/vpc/p4sln.h
    ${SRC_DIR}/utils/vpc/projectgenerator_xcode.h
    ${SRC_DIR}/utils/vpc/projectgenerator_ps3.h
    ${SRC_DIR}/utils/vpc/projectgenerator_win32_2010.h
    ${SRC_DIR}/utils/vpc/projectgenerator_win32.h
    ${SRC_DIR}/utils/vpc/projectgenerator_vcproj.h
    ${SRC_DIR}/utils/vpc/projectgenerator_xbox360.h
    ${SRC_DIR}/utils/vpc/projectgenerator_xbox360_2010.h
    ${SRC_DIR}/utils/vpc/scriptsource.h
    ${SRC_DIR}/utils/vpc/sys_utils.h
    ${SRC_DIR}/utils/vpc/vpc.h
    ${SRC_DIR}/utils/vpc/generatordefinition.h
)

add_library(utils_vpc_vpc SHARED ${UTILS_VPC_VPC_SOURCES})

valve_apply_base_settings(utils_vpc_vpc)

target_include_directories(utils_vpc_vpc PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_vpc_vpc PRIVATE
    _USRDLL
    _USE_32BIT_TIME_T
)

target_link_libraries(utils_vpc_vpc PRIVATE
    tier0
    tier1
    interfaces
    vstdlib
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    ws2_32.lib
)

set_target_properties(utils_vpc_vpc PROPERTIES OUTPUT_NAME "vpc")
valve_set_dll_importlib_output(utils_vpc_vpc)
valve_publish_dll(utils_vpc_vpc "${DEVTOOLS_BIN_DIR}")
