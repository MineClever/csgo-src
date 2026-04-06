# utils/fbx2dmx
# Generated from: fbx2dmx.vpc, vpc_scripts\source_exe_con_base.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\fbx.vpc, vpc_scripts\fbx_base.vpc

if(NOT VALVE_FBX_AVAILABLE)
    message(STATUS "utils/fbx2dmx: skipping (no FBX backend available)")
    return()
endif()

set(UTILS_FBX2DMX_FBX2DMX_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/fbx2dmx/main.cpp
)

add_executable(utils_fbx2dmx_fbx2dmx ${UTILS_FBX2DMX_FBX2DMX_SOURCES})

valve_apply_base_settings(utils_fbx2dmx_fbx2dmx)

target_include_directories(utils_fbx2dmx_fbx2dmx PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

if(VALVE_FBX_BACKEND_SDK)
    target_compile_definitions(utils_fbx2dmx_fbx2dmx PRIVATE VALVE_FBX_BACKEND_SDK)
elseif(VALVE_FBX_BACKEND_OPENFBX)
    target_compile_definitions(utils_fbx2dmx_fbx2dmx PRIVATE VALVE_FBX_BACKEND_OPENFBX)
endif()

target_link_libraries(utils_fbx2dmx_fbx2dmx PRIVATE
    tier0
    tier1
    interfaces
    vstdlib
    appframework
    bitmap
    bonesetup
    mathlib_extended
    mathlib
    Datamodel
    Dmserializers
    dmeutils_dmeutils
    fbxutils_fbxutils
    meshutils
    Movieobjects
    resourcefile
    tier2
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    wininet.lib
)

if(VALVE_FBX_BACKEND_SDK)
    target_link_libraries(utils_fbx2dmx_fbx2dmx PRIVATE valve_fbx_sdk)
endif()

set_target_properties(utils_fbx2dmx_fbx2dmx PROPERTIES OUTPUT_NAME "fbx2dmx")

valve_publish_exe(utils_fbx2dmx_fbx2dmx "${GAME_BIN_DIR}")
