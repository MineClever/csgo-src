# python/vpython
# Generated from: vpython.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(VSCRIPT_LANGUAGES_PYTHON_VPYTHON_VPYTHON_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vscript/languages/python/vpython/vpython.cpp
    ${SRC_DIR}/public/vscript/ivscript.h
    ${SRC_DIR}/public/vscript/vscript_templates.h
)

add_executable(vscript_languages_python_vpython_vpython ${VSCRIPT_LANGUAGES_PYTHON_VPYTHON_VPYTHON_SOURCES})

valve_apply_base_settings(vscript_languages_python_vpython_vpython)

target_include_directories(vscript_languages_python_vpython_vpython PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vscript/languages/python/python25/include
)

target_compile_definitions(vscript_languages_python_vpython_vpython PRIVATE
    PROTECTED_THINGS_DISABLE
    VPYTHON_TEST
)

target_link_libraries(vscript_languages_python_vpython_vpython PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
)

set_target_properties(vscript_languages_python_vpython_vpython PROPERTIES OUTPUT_NAME "vpython")

valve_publish_exe(vscript_languages_python_vpython_vpython "${GAME_BIN_DIR}")
