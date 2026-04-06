# lua/vlua
# Generated from: vlua.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(VSCRIPT_LANGUAGES_LUA_VLUA_VLUA_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vscript/languages/lua/vlua/vlua.cpp
    ${SRC_DIR}/vscript/languages/lua/vlua/vec3.cpp
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lapi.c
    ${SRC_DIR}/public/vscript/ivscript.h
    ${SRC_DIR}/public/vscript/vscript_templates.h
    ${SRC_DIR}/vscript/languages/lua/vlua/vec3.h
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lapi.h
)

add_executable(vscript_languages_lua_vlua_vlua ${VSCRIPT_LANGUAGES_LUA_VLUA_VLUA_SOURCES})

valve_apply_base_settings(vscript_languages_lua_vlua_vlua)

target_include_directories(vscript_languages_lua_vlua_vlua PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src
)

target_compile_definitions(vscript_languages_lua_vlua_vlua PRIVATE
    PROTECTED_THINGS_DISABLE
    VLUA_TEST
    MSVC
)

target_link_libraries(vscript_languages_lua_vlua_vlua PRIVATE
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

set_target_properties(vscript_languages_lua_vlua_vlua PROPERTIES OUTPUT_NAME "vlua")

valve_publish_exe(vscript_languages_lua_vlua_vlua "${GAME_BIN_DIR}")
