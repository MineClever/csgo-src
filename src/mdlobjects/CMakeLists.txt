# =============================================================================
# mdlobjects
# Generated from: mdlobjects.vcxproj
# =============================================================================

set(MDLOBJECTS_SOURCES
    authphysfx.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmeanimationassemblycommand.cpp
    dmeanimblocksize.cpp
    dmeanimcmd.cpp
    dmeassemblycommand.cpp
    dmeasset.cpp
    dmebbox.cpp
    dmeblankbodypart.cpp
    dmebodygroup.cpp
    dmebodygrouplist.cpp
    dmebodypart.cpp
    dmeboneflexdriver.cpp
    dmebonemask.cpp
    dmebonemasklist.cpp
    dmeboneweight.cpp
    dmecollisionjoints.cpp
    dmecollisionmodel.cpp
    dmedefinebone.cpp
    dmedefinebonelist.cpp
    dmeelementgroup.cpp
    dmeeyeball.cpp
    dmeeyeballglobals.cpp
    dmeeyelid.cpp
    dmehitbox.cpp
    dmehitboxset.cpp
    dmehitboxsetlist.cpp
    dmeik.cpp
    dmeincludemodellist.cpp
    dmejigglebone.cpp
    dmelod.cpp
    dmelodlist.cpp
    dmematerialgroup.cpp
    dmematerialgrouplist.cpp
    dmematsysroot.cpp
    dmemdllist.cpp
    dmemotioncontrol.cpp
    dmemouth.cpp
    dmeposeparameter.cpp
    dmeposeparameterlist.cpp
    dmeproceduralbone.cpp
    dmesequence.cpp
    dmesequencelist.cpp
    dmeskinner.cpp
    mpp_utils.cpp
    physmodelsource.cpp
)

add_library(mdlobjects STATIC ${MDLOBJECTS_SOURCES})

valve_apply_base_settings(mdlobjects)

target_compile_definitions(mdlobjects PRIVATE
    _LIB
    LIBNAME=mdlobjects
    MDLOBJECTS_LIB
)

valve_set_static_lib_output(mdlobjects)
