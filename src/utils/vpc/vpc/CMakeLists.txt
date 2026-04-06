# vpc/vpc
# Generated from: vpc.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_VPC_VPC_VPCNAME_SOURCES
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.cpp
    ${SRC_DIR}/common/clang/clang_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/clanggenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/conditionals.cpp
    ${SRC_DIR}/utils/vpc/vpc/configuration.cpp
    ${SRC_DIR}/utils/vpc/vpccrccheck/crccheck_shared.cpp
    ${SRC_DIR}/utils/vpc/vpc/dependencies.cpp
    ${SRC_DIR}/common/environment_utils.cpp
    ${SRC_DIR}/common/bundled_module_info.cpp
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.cpp
    ${SRC_DIR}/utils/vpc/vpc/generated_files.cpp
    ${SRC_DIR}/utils/vpc/vpc/groupscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/macros.cpp
    ${SRC_DIR}/utils/vpc/vpc/main.cpp
    ${SRC_DIR}/utils/vpc/vpc/p4sln.cpp
    ${SRC_DIR}/utils/vpc/vpc/pch_helpers.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/qtgenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/schemagenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_xcode.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/unity.cpp
    ${SRC_DIR}/utils/vpc/vpc/updateautoexp.cpp
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.h
    ${SRC_DIR}/common/clang/clang_utils.h
    ${SRC_DIR}/utils/vpc/vpc/dependencies.h
    ${SRC_DIR}/common/environment_utils.h
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.h
    ${SRC_DIR}/utils/vpc/vpc/ibaseprojectgenerator.h
    ${SRC_DIR}/utils/vpc/vpc/ibasesolutiongenerator.h
    ${SRC_DIR}/utils/vpc/vpc/p4sln.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.h
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.h
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.h
    ${SRC_DIR}/utils/vpc/vpc/vpc.h
)

add_library(utils_vpc_vpc_VPCNAME SHARED ${UTILS_VPC_VPC_VPCNAME_SOURCES})

valve_apply_base_settings(utils_vpc_vpc_VPCNAME)

target_include_directories(utils_vpc_vpc_VPCNAME PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/thirdparty/clang/include
)

target_compile_definitions(utils_vpc_vpc_VPCNAME PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    _USE_32BIT_TIME_T
)

target_link_libraries(utils_vpc_vpc_VPCNAME PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(utils_vpc_vpc_VPCNAME)
valve_publish_dll(utils_vpc_vpc_VPCNAME "${GAME_BIN_DIR}")

# vpc/vpc
# Generated from: vpcs.vpc, vpc.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_VPC_VPC_VPCNAME_SOURCES
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.cpp
    ${SRC_DIR}/common/clang/clang_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/clanggenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/conditionals.cpp
    ${SRC_DIR}/utils/vpc/vpc/configuration.cpp
    ${SRC_DIR}/utils/vpc/vpccrccheck/crccheck_shared.cpp
    ${SRC_DIR}/utils/vpc/vpc/dependencies.cpp
    ${SRC_DIR}/common/environment_utils.cpp
    ${SRC_DIR}/common/bundled_module_info.cpp
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.cpp
    ${SRC_DIR}/utils/vpc/vpc/generated_files.cpp
    ${SRC_DIR}/utils/vpc/vpc/groupscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/macros.cpp
    ${SRC_DIR}/utils/vpc/vpc/main.cpp
    ${SRC_DIR}/utils/vpc/vpc/p4sln.cpp
    ${SRC_DIR}/utils/vpc/vpc/pch_helpers.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/qtgenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/schemagenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_xcode.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/unity.cpp
    ${SRC_DIR}/utils/vpc/vpc/updateautoexp.cpp
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.h
    ${SRC_DIR}/common/clang/clang_utils.h
    ${SRC_DIR}/utils/vpc/vpc/dependencies.h
    ${SRC_DIR}/common/environment_utils.h
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.h
    ${SRC_DIR}/utils/vpc/vpc/ibaseprojectgenerator.h
    ${SRC_DIR}/utils/vpc/vpc/ibasesolutiongenerator.h
    ${SRC_DIR}/utils/vpc/vpc/p4sln.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.h
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.h
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.h
    ${SRC_DIR}/utils/vpc/vpc/vpc.h
)

add_library(utils_vpc_vpc_VPCNAME SHARED ${UTILS_VPC_VPC_VPCNAME_SOURCES})

valve_apply_base_settings(utils_vpc_vpc_VPCNAME)

target_include_directories(utils_vpc_vpc_VPCNAME PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/thirdparty/clang/include
)

target_compile_definitions(utils_vpc_vpc_VPCNAME PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    _USE_32BIT_TIME_T
)

target_link_libraries(utils_vpc_vpc_VPCNAME PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(utils_vpc_vpc_VPCNAME)
valve_publish_dll(utils_vpc_vpc_VPCNAME "${GAME_BIN_DIR}")

# vpc/vpc
# Generated from: vpcs_test.vpc, vpcs.vpc, vpc.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_VPC_VPC_VPCNAME_SOURCES
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.cpp
    ${SRC_DIR}/common/clang/clang_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/clanggenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/conditionals.cpp
    ${SRC_DIR}/utils/vpc/vpc/configuration.cpp
    ${SRC_DIR}/utils/vpc/vpccrccheck/crccheck_shared.cpp
    ${SRC_DIR}/utils/vpc/vpc/dependencies.cpp
    ${SRC_DIR}/common/environment_utils.cpp
    ${SRC_DIR}/common/bundled_module_info.cpp
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.cpp
    ${SRC_DIR}/utils/vpc/vpc/generated_files.cpp
    ${SRC_DIR}/utils/vpc/vpc/groupscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/macros.cpp
    ${SRC_DIR}/utils/vpc/vpc/main.cpp
    ${SRC_DIR}/utils/vpc/vpc/p4sln.cpp
    ${SRC_DIR}/utils/vpc/vpc/pch_helpers.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.cpp
    ${SRC_DIR}/utils/vpc/vpc/projectscript.cpp
    ${SRC_DIR}/utils/vpc/vpc/qtgenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/schemagenerator.cpp
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_makefile.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_xcode.cpp
    ${SRC_DIR}/utils/vpc/vpc/solutiongenerator_win32.cpp
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.cpp
    ${SRC_DIR}/utils/vpc/vpc/unity.cpp
    ${SRC_DIR}/utils/vpc/vpc/updateautoexp.cpp
    ${SRC_DIR}/utils/vpc/vpc/baseprojectdatacollector.h
    ${SRC_DIR}/common/clang/clang_utils.h
    ${SRC_DIR}/utils/vpc/vpc/dependencies.h
    ${SRC_DIR}/common/environment_utils.h
    ${SRC_DIR}/utils/vpc/vpc/generatordefinition.h
    ${SRC_DIR}/utils/vpc/vpc/ibaseprojectgenerator.h
    ${SRC_DIR}/utils/vpc/vpc/ibasesolutiongenerator.h
    ${SRC_DIR}/utils/vpc/vpc/p4sln.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_android.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_ps3.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_win32.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_vcproj.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_xbox360_2010.h
    ${SRC_DIR}/utils/vpc/vpc/projectgenerator_makefile.h
    ${SRC_DIR}/utils/vpc/vpc/scriptsource.h
    ${SRC_DIR}/utils/vpc/vpc/sys_utils.h
    ${SRC_DIR}/utils/vpc/vpc/vpc.h
)

add_library(utils_vpc_vpc_VPCNAME SHARED ${UTILS_VPC_VPC_VPCNAME_SOURCES})

valve_apply_base_settings(utils_vpc_vpc_VPCNAME)

target_include_directories(utils_vpc_vpc_VPCNAME PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/thirdparty/clang/include
)

target_compile_definitions(utils_vpc_vpc_VPCNAME PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    _USE_32BIT_TIME_T
)

target_link_libraries(utils_vpc_vpc_VPCNAME PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(utils_vpc_vpc_VPCNAME)
valve_publish_dll(utils_vpc_vpc_VPCNAME "${GAME_BIN_DIR}")
