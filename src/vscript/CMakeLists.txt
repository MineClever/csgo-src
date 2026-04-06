# =============================================================================
# vscript
# Generated from: vscript.vcxproj
# =============================================================================

set(VSCRIPT_SOURCES
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmArrayLib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmCall.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmGCRoot.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmGCRootUtil.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmHelpers.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmMathLib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmStringLib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmSystemLib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/binds/gmVector3Lib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmArraySimple.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmByteCode.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmByteCodeGen.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmCodeGen.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmCodeGenHooks.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmCodeTree.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmCrc.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmDebug.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmFunctionObject.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmHash.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmIncGC.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmLibHooks.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmListDouble.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmLog.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMachine.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMachineLib.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMem.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMemChain.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMemFixed.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmMemFixedSet.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmOperators.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmParser.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmScanner.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmStream.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmStreamBuffer.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmStringObject.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmTableObject.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmThread.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmUserObject.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmUtil.cpp
    ${SRC_DIR}/vscript/languages/gm/src/gm/gmVariable.cpp
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lapi.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lauxlib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lbaselib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lcode.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ldblib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ldebug.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ldo.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ldump.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lfunc.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lgc.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/linit.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/liolib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/llex.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lmathlib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lmem.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/loadlib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lobject.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lopcodes.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/loslib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lparser.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lstate.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lstring.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lstrlib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ltable.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ltablib.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/ltm.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lundump.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lvm.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/lzio.c
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src/print.c
    ${SRC_DIR}/vscript/languages/lua/vlua/vec3.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/vscript/languages/gm/vgm/vgm.cpp
    ${SRC_DIR}/vscript/languages/lua/vlua/vlua.cpp
    vscript.cpp
    ${SRC_DIR}/vscript/languages/squirrel/vsquirrel/vsquirrel.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SqPlus.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SquirrelBindingsUtils.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SquirrelBindingsUtilsWin32.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SquirrelObject.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqplus/SquirrelVM.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdaux.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdblob.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdmath.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdrex.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdstream.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqstdlib/sqstdstring.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqapi.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqbaselib.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqclass.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqcompiler.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqdbg/sqdbgserver.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqdebug.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqfuncstate.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqlexer.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqmem.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqobject.cpp
    ${SRC_DIR}/vscript/languages/squirrel/sqdbg/sqrdbg.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqstate.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqtable.cpp
    ${SRC_DIR}/vscript/languages/squirrel/squirrel/sqvm.cpp
)

add_library(vscript SHARED ${VSCRIPT_SOURCES})

valve_apply_base_settings(vscript)

target_include_directories(vscript PRIVATE
    ${SRC_DIR}/vscript/languages/gm/src/gm
    ${SRC_DIR}/vscript/languages/gm/src/binds
    ${SRC_DIR}/vscript/languages/gm/src/platform/win32msvc
    ${SRC_DIR}/vscript/languages/lua/lua-5.1.4/src
    ${SRC_DIR}/vscript/languages/squirrel/include
    ${SRC_DIR}/vscript/languages/squirrel/sqplus
)

target_compile_definitions(vscript PRIVATE
    _USRDLL
    DLLNAME=vscript
    MEMOVERRIDE_MODULE=vscript
    VSCRIPT_DLL_EXPORT
    LUA_MOD_CASE_INSENSITIVE
    MSVC
)

target_link_libraries(vscript PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    vstdlib
    mathlib
    tier2
    tier3
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(vscript)

valve_publish_dll(vscript "${GAME_BIN_DIR}")

add_subdirectory(languages)
