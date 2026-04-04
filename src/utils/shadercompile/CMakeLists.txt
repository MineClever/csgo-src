# =============================================================================
# Shadercompile_dll
# Generated from: shadercompile_dll.vcxproj
# =============================================================================

set(SHADERCOMPILE_DLL_SOURCES
    cfgprocessor.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    cmdsink.cpp
    d3dxfxc.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/common/pacifier.cpp
    shadercompile.cpp
    shadercompile_ps3_helpers.cpp
    subprocess.cpp
    ${SRC_DIR}/utils/common/threads.cpp
    ${SRC_DIR}/utils/common/tools_minidump.cpp
    ${SRC_DIR}/utils/common/vmpi_tools_shared.cpp
)

add_library(Shadercompile_dll SHARED ${SHADERCOMPILE_DLL_SOURCES})

valve_apply_base_settings(Shadercompile_dll)

target_include_directories(Shadercompile_dll PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/vmpi
    ${SRC_DIR}/dx9sdk/include
)

target_compile_definitions(Shadercompile_dll PRIVATE
    _USRDLL
    DLLNAME=shadercompile_dll
    MEMOVERRIDE_MODULE=shadercompile_dll
    SHADERCOMPILE_EXPORTS
    MPI
)

target_link_libraries(Shadercompile_dll PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(Shadercompile_dll)

valve_publish_dll(Shadercompile_dll "${GAME_BIN_DIR}")
