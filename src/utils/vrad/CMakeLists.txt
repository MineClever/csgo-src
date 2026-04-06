# =============================================================================
# Vrad_dll
# Generated from: vrad_dll.vcxproj
# =============================================================================

set(VRAD_DLL_SOURCES
    ${SRC_DIR}/public/bsptreedata.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/disp_common.cpp
    ${SRC_DIR}/public/disp_powerinfo.cpp
    disp_vrad.cpp
    imagepacker.cpp
    incremental.cpp
    leaf_ambient_lighting.cpp
    lightmap.cpp
    ${SRC_DIR}/public/loadcmdline.cpp
    ${SRC_DIR}/public/lumpfiles.cpp
    macro_texture.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/common/mpi_stats.cpp
    mpivrad.cpp
    ${SRC_DIR}/utils/common/MySqlDatabase.cpp
    ${SRC_DIR}/utils/common/pacifier.cpp
    ${SRC_DIR}/utils/common/physdll.cpp
    radial.cpp
    samplehash.cpp
    trace.cpp
    ${SRC_DIR}/utils/common/utilmatlib.cpp
    vismat.cpp
    ${SRC_DIR}/utils/common/vmpi_tools_shared.cpp
    vrad.cpp
    vrad_dispcoll.cpp
    vraddetailprops.cpp
    vraddisps.cpp
    vraddll.cpp
    vradstaticprops.cpp
    ${SRC_DIR}/public/zip_utils.cpp
    ${SRC_DIR}/utils/common/bsplib.cpp
    ${SRC_DIR}/public/builddisp.cpp
    ${SRC_DIR}/public/chunkfile.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/public/dispcoll_common.cpp
    ${SRC_DIR}/utils/common/map_shared.cpp
    ${SRC_DIR}/utils/common/polylib.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/common/threads.cpp
    ${SRC_DIR}/utils/common/tools_minidump.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp
    ${SRC_DIR}/public/ScratchPadUtils.cpp
)

add_library(Vrad_dll SHARED ${VRAD_DLL_SOURCES})

valve_apply_base_settings(Vrad_dll)

target_include_directories(Vrad_dll PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/vmpi
    ${SRC_DIR}/utils/vmpi/mysql/mysqlpp/include
    ${SRC_DIR}/utils/vmpi/mysql/include
)

target_compile_definitions(Vrad_dll PRIVATE
    _USRDLL
    DLLNAME=vrad_dll
    MEMOVERRIDE_MODULE=vrad_dll
    MPI
    PROTECTED_THINGS_DISABLE
    VRAD
)

target_link_libraries(Vrad_dll PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    mathlib
    vtf
    bitmap
    Raytrace
    prebuilt::vmpi
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(Vrad_dll)

valve_publish_dll(Vrad_dll "${GAME_BIN_DIR}")
