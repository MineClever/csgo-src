# =============================================================================
# Vvis_dll
# Generated from: vvis_dll.vcxproj
# =============================================================================

set(VVIS_DLL_SOURCES
    ${SRC_DIR}/utils/common/bsplib.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    flow.cpp
    ${SRC_DIR}/public/loadcmdline.cpp
    ${SRC_DIR}/public/lumpfiles.cpp
    ${SRC_DIR}/utils/common/mpi_stats.cpp
    mpivis.cpp
    ${SRC_DIR}/utils/common/MySqlDatabase.cpp
    ${SRC_DIR}/utils/common/pacifier.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp
    ${SRC_DIR}/utils/common/scratchpad_helpers.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/common/threads.cpp
    ${SRC_DIR}/utils/common/tools_minidump.cpp
    ${SRC_DIR}/utils/common/vmpi_tools_shared.cpp
    vvis.cpp
    WaterDist.cpp
    ${SRC_DIR}/public/zip_utils.cpp
)

add_library(Vvis_dll SHARED ${VVIS_DLL_SOURCES})

valve_apply_base_settings(Vvis_dll)

target_include_directories(Vvis_dll PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/vmpi
    ${SRC_DIR}/utils/vmpi/mysql/include
)

target_compile_definitions(Vvis_dll PRIVATE
    _USRDLL
    DLLNAME=vvis_dll
    MEMOVERRIDE_MODULE=vvis_dll
    MPI
    PROTECTED_THINGS_DISABLE
)

target_link_libraries(Vvis_dll PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    mathlib
    prebuilt::vmpi
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

valve_set_dll_importlib_output(Vvis_dll)

valve_publish_dll(Vvis_dll "${GAME_BIN_DIR}")
