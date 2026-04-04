# =============================================================================
# Hlmv
# Generated from: hlmv.vcxproj
# =============================================================================

set(HLMV_SOURCES
    attachments_window.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    controlpanel.cpp
    debugdrawmodel.cpp
    fileassociation.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/jigglebones.cpp
    matsyswin.cpp
    ${SRC_DIR}/public/mdlobjects/mdlobjects.cpp
    mdlviewer.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/movieobjects/movieobjects_compiletools.cpp
    mxLineEdit2.cpp
    pakviewer.cpp
    physmesh.cpp
    ${SRC_DIR}/public/studio.cpp
    studio_flex.cpp
    studio_render.cpp
    studio_utils.cpp
    sys_win.cpp
    viewersettings.cpp
)

add_executable(Hlmv ${HLMV_SOURCES})

valve_apply_base_settings(Hlmv)

target_include_directories(Hlmv PRIVATE
    ${SRC_DIR}/common
    ${UTILS_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Hlmv PRIVATE
    MEMOVERRIDE_MODULE=hlmv
    VECTOR
    PROTECTED_THINGS_DISABLE
    ENABLE_STUDIO_SOFTBODY
)

target_link_libraries(Hlmv PRIVATE
    # 系统库
    advapi32.lib
    comctl32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
)

valve_publish_exe(Hlmv "${GAME_DIR}")
