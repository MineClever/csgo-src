# vgui2/vgui_perftest
# Generated from: vgui_perftest.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(VGUI2_VGUI_PERFTEST_VGUIPERFTEST_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vgui2/vgui_perftest/amalg_texture_parser.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/vgui2/vgui_perftest/vgui_perftest.cpp
    ${SRC_DIR}/vgui2/vgui_perftest/vguimaterial.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/public/materialobjects/materialobjects.cpp
    ${SRC_DIR}/vgui2/vgui_perftest/mksutil.cpp
    ${SRC_DIR}/public/interpolatortypes.h
    ${SRC_DIR}/vgui2/vgui_perftest/amalg_texture_parser.h
    ${SRC_DIR}/utils/common/cmdlib.h
    ${SRC_DIR}/public/FileSystem.h
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/vgui2/vgui_perftest/vguimaterial.h
    ${SRC_DIR}/public/pixelwriter.h
    ${SRC_DIR}/vgui2/vgui_perftest/mksutil.h
    ${SRC_DIR}/vgui2/game_controls/GameUISystem.h
)

add_executable(vgui2_vgui_perftest_vguiPerfTest ${VGUI2_VGUI_PERFTEST_VGUIPERFTEST_SOURCES})

valve_apply_base_settings(vgui2_vgui_perftest_vguiPerfTest)

target_include_directories(vgui2_vgui_perftest_vguiPerfTest PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vgui2/include
    ${SRC_DIR}/vgui2/controls
    ${SRC_DIR}/vgui2/common
)

target_link_libraries(vgui2_vgui_perftest_vguiPerfTest PRIVATE
    tier1
    interfaces
    bitmap
    Datamodel
    Dmserializers
    Raytrace
    appframework
    Dmxloader
    game_controls
    Materialobjects
    mathlib
    matsys_controls
    resourcefile
    tier2
    tier3
    vtf
    vgui_controls
    vgui_surfacelib
    videocfg
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    comctl32.lib
    odbc32.lib
    odbccp32.lib
    Rpcrt4.lib
    Imm32.lib
)

set_target_properties(vgui2_vgui_perftest_vguiPerfTest PROPERTIES OUTPUT_NAME "vguiPerfTest")

valve_publish_exe(vgui2_vgui_perftest_vguiPerfTest "${GAME_BIN_DIR}")
