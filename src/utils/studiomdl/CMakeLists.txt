# utils/studiomdl
# Generated from: mdlcompile.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\fbx.vpc, vpc_scripts\fbx_base.vpc

if(NOT VALVE_FBX_AVAILABLE)
    message(STATUS "utils/studiomdl: skipping (no FBX backend available)")
    return()
endif()

set(UTILS_STUDIOMDL_EDGEGEOM_LIB "${SRC_DIR}/utils/studiomdl/libedgegeomtool.Release.Win32.vs8.lib")
if(EXISTS "${UTILS_STUDIOMDL_EDGEGEOM_LIB}")
    set(UTILS_STUDIOMDL_EDGEGEOM_LINK "${UTILS_STUDIOMDL_EDGEGEOM_LIB}")
else()
    set(UTILS_STUDIOMDL_EDGEGEOM_LINK "")
    message(STATUS "utils/studiomdl: libedgegeomtool not found, building without edgegeom support")
endif()

set(UTILS_STUDIOMDL_MDLCOMPILE_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/utils/studiomdl/collisionmodel.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/utils/studiomdl/collisionmodelsource.cpp
    ${SRC_DIR}/utils/common/datalinker.cpp
    ${SRC_DIR}/utils/studiomdl/dmxsupport.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/utils/studiomdl/hardwarematrixstate.cpp
    ${SRC_DIR}/utils/studiomdl/hardwarevertexcache.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/movieobjects/movieobjects_compiletools.cpp
    ${SRC_DIR}/public/mdlobjects/mdlobjects.cpp
    ${SRC_DIR}/utils/studiomdl/mrmsupport.cpp
    ${SRC_DIR}/utils/studiomdl/objsupport.cpp
    ${SRC_DIR}/utils/studiomdl/optimize.cpp
    ${SRC_DIR}/utils/studiomdl/optimize_subd.cpp
    ${SRC_DIR}/utils/studiomdl/perfstats.cpp
    ${SRC_DIR}/utils/common/physdll.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/studiomdl/simplify.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/common/studiobyteswap.cpp
    ${SRC_DIR}/utils/studiomdl/studiomdl.cpp
    ${SRC_DIR}/utils/studiomdl/UnifyLODs.cpp
    ${SRC_DIR}/utils/studiomdl/v1support.cpp
    ${SRC_DIR}/utils/studiomdl/write.cpp
    ${SRC_DIR}/utils/studiomdl/compileclothproxy.cpp
    ${SRC_DIR}/utils/studiomdl/compileclothproxy.h
    ${SRC_DIR}/utils/common/cmdlib.h
    ${SRC_DIR}/utils/studiomdl/collisionmodel.h
    ${SRC_DIR}/utils/studiomdl/collisionmodelsource.h
    ${SRC_DIR}/utils/studiomdl/filebuffer.h
    ${SRC_DIR}/utils/common/datalinker.h
    ${SRC_DIR}/utils/common/filesystem_tools.h
    ${SRC_DIR}/utils/studiomdl/hardwarematrixstate.h
    ${SRC_DIR}/utils/studiomdl/hardwarevertexcache.h
    ${SRC_DIR}/utils/nvtristriplib/nvtristrip.h
    ${SRC_DIR}/utils/studiomdl/perfstats.h
    ${SRC_DIR}/utils/common/physdll.h
    ${SRC_DIR}/utils/common/scriplib.h
    ${SRC_DIR}/utils/studiomdl/studiomdl.h
    ${SRC_DIR}/utils/studiomdl/optimize_subd.h
    ${SRC_DIR}/public/gametrace.h
    ${SRC_DIR}/public/filesystem.h
    ${SRC_DIR}/public/filesystem_helpers.h
    ${SRC_DIR}/public/cmodel.h
    ${SRC_DIR}/public/basehandle.h
    ${SRC_DIR}/public/tier0/basetypes.h
    ${SRC_DIR}/public/bitvec.h
    ${SRC_DIR}/public/bone_accessor.h
    ${SRC_DIR}/public/bone_setup.h
    ${SRC_DIR}/public/bspflags.h
    ${SRC_DIR}/public/tier1/byteswap.h
    ${SRC_DIR}/public/tier1/characterset.h
    ${SRC_DIR}/public/collisionutils.h
    ${SRC_DIR}/public/mathlib/compressed_vector.h
    ${SRC_DIR}/public/const.h
    ${SRC_DIR}/public/vphysics/constraints.h
    ${SRC_DIR}/public/tier0/dbg.h
    ${SRC_DIR}/public/tier0/fasttimer.h
    ${SRC_DIR}/public/appframework/iappsystem.h
    ${SRC_DIR}/public/tier0/icommandline.h
    ${SRC_DIR}/public/ihandleentity.h
    ${SRC_DIR}/public/materialsystem/imaterial.h
    ${SRC_DIR}/public/materialsystem/imaterialsystem.h
    ${SRC_DIR}/public/materialsystem/imaterialvar.h
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/public/istudiorender.h
    ${SRC_DIR}/public/tier1/keyvalues.h
    ${SRC_DIR}/public/materialsystem/materialsystem_config.h
    ${SRC_DIR}/public/mathlib/mathlib.h
    ${SRC_DIR}/public/tier0/memdbgoff.h
    ${SRC_DIR}/public/tier0/memdbgon.h
    ${SRC_DIR}/public/phyfile.h
    ${SRC_DIR}/public/optimize.h
    ${SRC_DIR}/public/tier0/platform.h
    ${SRC_DIR}/public/vstdlib/random.h
    ${SRC_DIR}/common/studiobyteswap.h
    ${SRC_DIR}/public/string_t.h
    ${SRC_DIR}/public/tier1/strtools.h
    ${SRC_DIR}/public/studio.h
    ${SRC_DIR}/public/tier3/tier3.h
    ${SRC_DIR}/public/tier1/utlbuffer.h
    ${SRC_DIR}/public/tier1/utldict.h
    ${SRC_DIR}/public/tier1/utllinkedlist.h
    ${SRC_DIR}/public/tier1/utlmemory.h
    ${SRC_DIR}/public/tier1/utlrbtree.h
    ${SRC_DIR}/public/tier1/utlsymbol.h
    ${SRC_DIR}/public/tier1/utlvector.h
    ${SRC_DIR}/public/vcollide.h
    ${SRC_DIR}/public/vcollide_parse.h
    ${SRC_DIR}/public/mathlib/vector.h
    ${SRC_DIR}/public/mathlib/vector2d.h
    ${SRC_DIR}/public/mathlib/vector4d.h
    ${SRC_DIR}/public/mathlib/vmatrix.h
    ${SRC_DIR}/public/vphysics_interface.h
    ${SRC_DIR}/public/mathlib/vplane.h
    ${SRC_DIR}/public/tier0/vprof.h
    ${SRC_DIR}/public/vstdlib/vstdlib.h
)

add_executable(utils_studiomdl_mdlcompile ${UTILS_STUDIOMDL_MDLCOMPILE_SOURCES})

valve_apply_base_settings(utils_studiomdl_mdlcompile)

if(VALVE_FBX_BACKEND_SDK)
    target_compile_definitions(utils_studiomdl_mdlcompile PRIVATE VALVE_FBX_BACKEND_SDK)
elseif(VALVE_FBX_BACKEND_OPENFBX)
    target_compile_definitions(utils_studiomdl_mdlcompile PRIVATE VALVE_FBX_BACKEND_OPENFBX)
endif()

target_include_directories(utils_studiomdl_mdlcompile PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/nvtristriplib
    ${SRC_DIR}/Game_Shared
    ${SRC_DIR}/ps3sdk/cell/host-common/include
    ${SRC_DIR}/ps3sdk/cell/target/common/include
)

target_compile_definitions(utils_studiomdl_mdlcompile PRIVATE PROTECTED_THINGS_DISABLE MDLCOMPILE)

target_link_libraries(utils_studiomdl_mdlcompile PRIVATE
    tier0
    tier1
    interfaces
    appframework
    bonesetup
    Datamodel
    dmeutils_dmeutils
    Dmserializers
    mathlib
    mathlib_extended
    resourcefile
    mdlobjects
    meshutils
    Movieobjects
    prebuilt::nvtristrip
    tier2
    tier3
    vstdlib
    fbxutils_fbxutils
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    wininet.lib
    winmm.lib
    ${UTILS_STUDIOMDL_EDGEGEOM_LINK}
)

if(VALVE_FBX_BACKEND_SDK)
    target_link_libraries(utils_studiomdl_mdlcompile PRIVATE valve_fbx_sdk)
endif()

set_target_properties(utils_studiomdl_mdlcompile PROPERTIES OUTPUT_NAME "mdlcompile")

valve_publish_exe(utils_studiomdl_mdlcompile "${GAME_BIN_DIR}")

# utils/studiomdl
# Generated from: studiomdl.vpc, vpc_scripts\source_exe_con_win32_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_exe_win_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_exe_win_win32_debug.vpc, vpc_scripts\source_exe_win_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\fbx.vpc, vpc_scripts\fbx_base.vpc

set(UTILS_STUDIOMDL_STUDIOMDL_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/utils/common/datalinker.cpp
    ${SRC_DIR}/utils/studiomdl/collisionmodel.cpp
    ${SRC_DIR}/utils/studiomdl/collisionmodelsource.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/utils/studiomdl/dmxsupport.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    ${SRC_DIR}/utils/studiomdl/hardwarematrixstate.cpp
    ${SRC_DIR}/utils/studiomdl/hardwarevertexcache.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/mdlobjects/mdlobjects.cpp
    ${SRC_DIR}/public/movieobjects/movieobjects_compiletools.cpp
    ${SRC_DIR}/utils/studiomdl/mrmsupport.cpp
    ${SRC_DIR}/utils/studiomdl/objsupport.cpp
    ${SRC_DIR}/utils/studiomdl/optimize_subd.cpp
    ${SRC_DIR}/utils/studiomdl/optimize.cpp
    ${SRC_DIR}/utils/studiomdl/perfstats.cpp
    ${SRC_DIR}/utils/common/physdll.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/utils/studiomdl/simplify.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/common/studiobyteswap.cpp
    ${SRC_DIR}/utils/studiomdl/studiomdl.cpp
    ${SRC_DIR}/utils/studiomdl/compileclothproxy.cpp
    ${SRC_DIR}/utils/studiomdl/UnifyLODs.cpp
    ${SRC_DIR}/utils/studiomdl/v1support.cpp
    ${SRC_DIR}/utils/studiomdl/write.cpp
    ${SRC_DIR}/utils/common/cmdlib.h
    ${SRC_DIR}/utils/common/datalinker.h
    ${SRC_DIR}/utils/studiomdl/collisionmodel.h
    ${SRC_DIR}/utils/studiomdl/collisionmodelsource.h
    ${SRC_DIR}/utils/studiomdl/physics2collision.h
    ${SRC_DIR}/utils/studiomdl/filebuffer.h
    ${SRC_DIR}/utils/common/filesystem_tools.h
    ${SRC_DIR}/utils/studiomdl/hardwarematrixstate.h
    ${SRC_DIR}/utils/studiomdl/hardwarevertexcache.h
    ${SRC_DIR}/utils/nvtristriplib/nvtristrip.h
    ${SRC_DIR}/utils/studiomdl/perfstats.h
    ${SRC_DIR}/utils/common/physdll.h
    ${SRC_DIR}/utils/common/scriplib.h
    ${SRC_DIR}/utils/studiomdl/studiomdl.h
    ${SRC_DIR}/utils/studiomdl/optimize_subd.h
    ${SRC_DIR}/utils/studiomdl/compileclothproxy.h
    ${SRC_DIR}/public/alignedarray.h
    ${SRC_DIR}/public/gametrace.h
    ${SRC_DIR}/public/filesystem.h
    ${SRC_DIR}/public/filesystem_helpers.h
    ${SRC_DIR}/public/cmodel.h
    ${SRC_DIR}/public/basehandle.h
    ${SRC_DIR}/public/tier0/basetypes.h
    ${SRC_DIR}/public/bitvec.h
    ${SRC_DIR}/public/bone_accessor.h
    ${SRC_DIR}/public/bone_setup.h
    ${SRC_DIR}/public/bspflags.h
    ${SRC_DIR}/public/tier1/byteswap.h
    ${SRC_DIR}/public/tier1/characterset.h
    ${SRC_DIR}/public/collisionutils.h
    ${SRC_DIR}/public/mathlib/compressed_vector.h
    ${SRC_DIR}/public/const.h
    ${SRC_DIR}/public/vphysics/constraints.h
    ${SRC_DIR}/public/tier0/dbg.h
    ${SRC_DIR}/public/tier0/fasttimer.h
    ${SRC_DIR}/public/appframework/iappsystem.h
    ${SRC_DIR}/public/tier0/icommandline.h
    ${SRC_DIR}/public/ihandleentity.h
    ${SRC_DIR}/public/materialsystem/imaterial.h
    ${SRC_DIR}/public/materialsystem/imaterialsystem.h
    ${SRC_DIR}/public/materialsystem/imaterialvar.h
    ${SRC_DIR}/public/tier1/interface.h
    ${SRC_DIR}/public/istudiorender.h
    ${SRC_DIR}/public/tier1/keyvalues.h
    ${SRC_DIR}/public/materialsystem/materialsystem_config.h
    ${SRC_DIR}/public/mathlib/mathlib.h
    ${SRC_DIR}/public/tier0/memdbgoff.h
    ${SRC_DIR}/public/tier0/memdbgon.h
    ${SRC_DIR}/public/phyfile.h
    ${SRC_DIR}/public/phzfile.h
    ${SRC_DIR}/public/optimize.h
    ${SRC_DIR}/public/tier0/platform.h
    ${SRC_DIR}/public/vstdlib/random.h
    ${SRC_DIR}/common/studiobyteswap.h
    ${SRC_DIR}/public/string_t.h
    ${SRC_DIR}/public/tier1/strtools.h
    ${SRC_DIR}/public/studio.h
    ${SRC_DIR}/public/tier3/tier3.h
    ${SRC_DIR}/public/tier1/utlbuffer.h
    ${SRC_DIR}/public/tier1/utldict.h
    ${SRC_DIR}/public/tier1/utllinkedlist.h
    ${SRC_DIR}/public/tier1/utlmemory.h
    ${SRC_DIR}/public/tier1/utlrbtree.h
    ${SRC_DIR}/public/tier1/utlsymbol.h
    ${SRC_DIR}/public/tier1/utlvector.h
    ${SRC_DIR}/public/vcollide.h
    ${SRC_DIR}/public/vcollide_parse.h
    ${SRC_DIR}/public/mathlib/vector.h
    ${SRC_DIR}/public/mathlib/vector2d.h
    ${SRC_DIR}/public/mathlib/vector4d.h
    ${SRC_DIR}/public/mathlib/vmatrix.h
    ${SRC_DIR}/public/vphysics_interface.h
    ${SRC_DIR}/public/mathlib/vplane.h
    ${SRC_DIR}/public/tier0/vprof.h
    ${SRC_DIR}/public/vstdlib/vstdlib.h
)

add_executable(utils_studiomdl_Studiomdl ${UTILS_STUDIOMDL_STUDIOMDL_SOURCES})

valve_apply_base_settings(utils_studiomdl_Studiomdl)

target_include_directories(utils_studiomdl_Studiomdl PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/nvtristriplib
    ${SRC_DIR}/Game_Shared
    ${SRC_DIR}/ps3sdk/cell/host-common/include
    ${SRC_DIR}/ps3sdk/cell/target/common/include
)

target_compile_definitions(utils_studiomdl_Studiomdl PRIVATE PROTECTED_THINGS_DISABLE)

target_link_libraries(utils_studiomdl_Studiomdl PRIVATE
    tier0
    tier1
    interfaces
    resourcefile
    dmeutils_dmeutils
    meshutils
    appframework
    bonesetup
    Datamodel
    Dmserializers
    mathlib
    mathlib_extended
    mdlobjects
    Movieobjects
    prebuilt::nvtristrip
    tier2
    tier3
    vstdlib
    fbxutils_fbxutils
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    wininet.lib
    winmm.lib
    ${UTILS_STUDIOMDL_EDGEGEOM_LINK}
)

if(VALVE_FBX_BACKEND_SDK)
    target_link_libraries(utils_studiomdl_Studiomdl PRIVATE valve_fbx_sdk)
endif()

set_target_properties(utils_studiomdl_Studiomdl PROPERTIES OUTPUT_NAME "Studiomdl")

valve_publish_exe(utils_studiomdl_Studiomdl "${GAME_BIN_DIR}")
