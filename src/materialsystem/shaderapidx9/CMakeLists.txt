# =============================================================================
# shaderapidx9
# Generated from: shaderapidx9.vcxproj
# =============================================================================

set(SHADERAPIDX9_SOURCES
    colorformatdx8.cpp
    cvballoctracker.cpp
    d3d_async.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    hardwareconfig.cpp
    meshbase.cpp
    meshdx8.cpp
    recording.cpp
    shaderapibase.cpp
    shaderapidx8.cpp
    shaderdevicebase.cpp
    shaderdevicedx8.cpp
    shadershadowdx8.cpp
    texturedx8.cpp
    TransitionTable.cpp
    vertexdecl.cpp
    vertexshaderdx8.cpp
    wmi.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
)

add_library(shaderapidx9 SHARED ${SHADERAPIDX9_SOURCES})

valve_apply_base_settings(shaderapidx9)

target_include_directories(shaderapidx9 PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/dx9sdk/include
    ${SRC_DIR}/materialsystem
    ${SRC_DIR}/thirdparty/nvidia/nvapi
)

target_compile_definitions(shaderapidx9 PRIVATE
    _USRDLL
    DLLNAME=shaderapidx9
    MEMOVERRIDE_MODULE=shaderapidx9
    SHADERAPIDX9
    SHADER_DLL_EXPORT
    PROTECTED_THINGS_ENABLE
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    USE_ACTUAL_DX
)

target_link_libraries(shaderapidx9 PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    bitmap
    mathlib
    vtf
    videocfg
    prebuilt::bzip2
    prebuilt::nvapi
    prebuilt::d3dx9
    # 系统库
    d3d9.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(shaderapidx9)

valve_publish_dll(shaderapidx9 "${GAME_BIN_DIR}")
