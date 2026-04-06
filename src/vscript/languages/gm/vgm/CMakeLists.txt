# gm/vgm
# Generated from: vgm.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(VSCRIPT_LANGUAGES_GM_VGM_VGM_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vscript/languages/gm/vgm/vgm.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmArraySimple.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmArrayLib.cpp
    ${SRC_DIR}/public/vscript/ivscript.h
    ${SRC_DIR}/public/vscript/vscript_templates.h
    ${SRC_DIR}/vscript/languages/gm/src/platform/win32msvc/gmConfig_p.h
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmArrayLib.h
)

add_executable(vscript_languages_gm_vgm_vgm ${VSCRIPT_LANGUAGES_GM_VGM_VGM_SOURCES})

valve_apply_base_settings(vscript_languages_gm_vgm_vgm)

target_include_directories(vscript_languages_gm_vgm_vgm PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vscript/languages/gm/src/gm
    ${SRC_DIR}/vscript/languages/gm/src/binds
    ${SRC_DIR}/vscript/languages/gm/src/platform/win32msvc
)

target_compile_definitions(vscript_languages_gm_vgm_vgm PRIVATE
    PROTECTED_THINGS_DISABLE
    VGM_TEST
)

target_link_libraries(vscript_languages_gm_vgm_vgm PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
)

set_target_properties(vscript_languages_gm_vgm_vgm PROPERTIES OUTPUT_NAME "vgm")

valve_publish_exe(vscript_languages_gm_vgm_vgm "${GAME_BIN_DIR}")
