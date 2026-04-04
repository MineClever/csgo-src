# =============================================================================
# Vbsp
# Generated from: vbsp.vcxproj
# =============================================================================

set(VBSP_SOURCES
    boundbox.cpp
    brushbsp.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    csg.cpp
    cubemap.cpp
    detail.cpp
    detailobjects.cpp
    ${SRC_DIR}/public/disp_common.cpp
    disp_ivp.cpp
    ${SRC_DIR}/public/disp_powerinfo.cpp
    disp_vbsp.cpp
    faces.cpp
    glfile.cpp
    ivp.cpp
    leakfile.cpp
    ${SRC_DIR}/public/loadcmdline.cpp
    ${SRC_DIR}/public/lumpfiles.cpp
    manifest.cpp
    map.cpp
    materialpatch.cpp
    materialsub.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/common/mstristrip.cpp
    nodraw.cpp
    normals.cpp
    overlay.cpp
    ${SRC_DIR}/utils/common/physdll.cpp
    portals.cpp
    prtfile.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp
    ${SRC_DIR}/utils/common/scratchpad_helpers.cpp
    staticprop.cpp
    textures.cpp
    tree.cpp
    ${SRC_DIR}/utils/common/utilmatlib.cpp
    vbsp.cpp
    worldvertextransitionfixup.cpp
    writebsp.cpp
    ${SRC_DIR}/public/zip_utils.cpp
    ${SRC_DIR}/utils/common/bsplib.cpp
    ${SRC_DIR}/public/builddisp.cpp
    ${SRC_DIR}/public/chunkfile.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/utils/common/map_shared.cpp
    ${SRC_DIR}/utils/common/pacifier.cpp
    ${SRC_DIR}/utils/common/polylib.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/common/threads.cpp
    ${SRC_DIR}/utils/common/tools_minidump.cpp
    ${SRC_DIR}/common/vmfentitysupport.cpp
    ${SRC_DIR}/common/vmfmeshdatasupport.cpp
)

add_executable(Vbsp ${VBSP_SOURCES})

valve_apply_base_settings(Vbsp)

target_include_directories(Vbsp PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/vmpi
)

target_compile_definitions(Vbsp PRIVATE
    MEMOVERRIDE_MODULE=vbsp
    MACRO_MATHLIB
    PROTECTED_THINGS_DISABLE
)

target_link_libraries(Vbsp PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
    ws2_32.lib
)

valve_publish_exe(Vbsp "${GAME_DIR}")
