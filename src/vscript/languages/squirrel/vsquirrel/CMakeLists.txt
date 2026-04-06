# squirrel/vsquirrel
# Generated from: vsquirrel.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(VSCRIPT_LANGUAGES_SQUIRREL_VSQUIRREL_VSQUIRREL_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vscript/languages/squirrel/vsquirrel/vsquirrel.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqapi.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdaux.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SqPlus.cpp
    ${SRC_DIR}/public/vscript/ivscript.h
    ${SRC_DIR}/public/vscript/vscript_templates.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqdbgserver.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqrdbg.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdaux.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdblob.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdio.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdmath.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdstring.h
    ${SRC_DIR}/vscript/languages/squirrel/include/sqstdsystem.h
    ${SRC_DIR}/vscript/languages/squirrel/include/squirrel.h
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqarray.h
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/sqplus.h
)

add_executable(vscript_languages_squirrel_vsquirrel_vsquirrel ${VSCRIPT_LANGUAGES_SQUIRREL_VSQUIRREL_VSQUIRREL_SOURCES})

valve_apply_base_settings(vscript_languages_squirrel_vsquirrel_vsquirrel)

target_include_directories(vscript_languages_squirrel_vsquirrel_vsquirrel PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vscript/languages/squirrel/include
    ${SRC_DIR}/vscript/languages/squirrel/sqplus
)

target_compile_definitions(vscript_languages_squirrel_vsquirrel_vsquirrel PRIVATE
    PROTECTED_THINGS_DISABLE
    VSQUIRREL_TEST
)

target_link_libraries(vscript_languages_squirrel_vsquirrel_vsquirrel PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    mathlib
)

set_target_properties(vscript_languages_squirrel_vsquirrel_vsquirrel PROPERTIES OUTPUT_NAME "vsquirrel")

valve_publish_exe(vscript_languages_squirrel_vsquirrel_vsquirrel "${GAME_BIN_DIR}")
