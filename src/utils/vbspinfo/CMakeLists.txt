# =============================================================================
# Vbspinfo
# Generated from: vbspinfo.vcxproj
# =============================================================================

set(VBSPINFO_SOURCES
    ${SRC_DIR}/utils/common/bsplib.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/public/lumpfiles.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/common/tools_minidump.cpp
    vbspinfo.cpp
    ${SRC_DIR}/public/zip_utils.cpp
)

add_executable(Vbspinfo ${VBSPINFO_SOURCES})

valve_apply_base_settings(Vbspinfo)

target_include_directories(Vbspinfo PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
)

target_compile_definitions(Vbspinfo PRIVATE
    MEMOVERRIDE_MODULE=vbspinfo
    DONT_PROTECT_FILEIO_FUNCTIONS
)

target_link_libraries(Vbspinfo PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    mathlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Vbspinfo "${GAME_BIN_DIR}")
