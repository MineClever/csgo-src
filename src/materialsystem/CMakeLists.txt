# =============================================================================
# materialsystem
# Generated from: materialsystem.vcxproj
# =============================================================================

set(MATERIALSYSTEM_SOURCES
    base_visuals_data_processor.cpp
    CColorCorrection.cpp
    cmaterial.cpp
    cmaterial_queuefriendly.cpp
    cmaterialdict.cpp
    CMaterialSubRect.cpp
    cmaterialsystem.cpp
    cmaterialvar.cpp
    cmatlightmaps.cpp
    cmatnullrendercontext.cpp
    cmatpaintmaps.cpp
    cmatqueuedrendercontext.cpp
    cmatrendercontext.cpp
    colorspace.cpp
    composite_texture.cpp
    ctexture.cpp
    custom_material.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    imagepacker.cpp
    mat_stub.cpp
    materialsystem_global.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    morph.cpp
    occlusionquerymgr.cpp
    pch_materialsystem.cpp
    shadersystem.cpp
    SubdMgr.cpp
    texturemanager.cpp
)

add_library(materialsystem SHARED ${MATERIALSYSTEM_SOURCES})

valve_apply_base_settings(materialsystem)

target_include_directories(materialsystem PRIVATE
    ${SRC_DIR}/materialsystem
    ${SRC_DIR}/dx9sdk/include
)

target_compile_definitions(materialsystem PRIVATE
    _USRDLL
    DLLNAME=materialsystem
    MEMOVERRIDE_MODULE=materialsystem
    DEFINE_MATERIALSYSTEM_INTERFACE
    MATERIALSYSTEM_EXPORTS
    PROTECTED_THINGS_ENABLE
    VERSION_SAFE_STEAM_API_INTERFACES
    USE_ACTUAL_DX
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
)

target_link_libraries(materialsystem PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    bitmap
    mathlib
    shaderlib
    vtf
    videocfg
    prebuilt::d3dx9
    prebuilt::d3dx9d
    # 系统库
    Ws2_32.lib
    advapi32.lib
    comdlg32.lib
    d3d9.lib
    dxguid.lib
    gdi32.lib
    imm32.lib
    ole32.lib
    oleaut32.lib
    shell32.lib
    user32.lib
    winmm.lib
)

valve_set_dll_importlib_output(materialsystem)

valve_publish_dll(materialsystem "${GAME_BIN_DIR}")


add_subdirectory(shaderapidx9)
add_subdirectory(shaderapiempty)
add_subdirectory(shaderlib)
add_subdirectory(stdshaders)