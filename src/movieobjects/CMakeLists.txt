# =============================================================================
# Movieobjects
# Generated from: movieobjects.vcxproj
# =============================================================================

set(MOVIEOBJECTS_SOURCES
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmeanimationlist.cpp
    dmeanimationset.cpp
    dmeattachment.cpp
    dmeattributereference.cpp
    dmeaxissystem.cpp
    dmebookmark.cpp
    dmecamera.cpp
    dmechannel.cpp
    dmeclip.cpp
    dmecombinationoperator.cpp
    dmeconnectionoperator.cpp
    dmecontrolgroup.cpp
    dmedag.cpp
    dmedccmakefile.cpp
    dmedrawsettings.cpp
    dmeeditortypedictionary.cpp
    dmeexporttags.cpp
    dmeexpressionoperator.cpp
    dmefaceset.cpp
    dmeflexrules.cpp
    dmegamemodel.cpp
    dmeinput.cpp
    dmejoint.cpp
    dmekeyboardinput.cpp
    dmelight.cpp
    dmelog.cpp
    dmemakefile.cpp
    dmemakefileutils.cpp
    dmematerial.cpp
    dmematerialoverlayfxclip.cpp
    dmemdl.cpp
    dmemdlmakefile.cpp
    dmemesh.cpp
    dmemodel.cpp
    dmemorphoperator.cpp
    dmemouseinput.cpp
    dmeoperator.cpp
    dmeoverlay.cpp
    dmepackoperators.cpp
    dmeparticlesystemdefinition.cpp
    dmephonememapping.cpp
    dmerig.cpp
    dmerigconstraintoperators.cpp
    dmerighandle.cpp
    dmeselection.cpp
    dmeshape.cpp
    dmesound.cpp
    dmetimeframe.cpp
    dmetimeselection.cpp
    dmetrack.cpp
    dmetrackgroup.cpp
    dmetransform.cpp
    dmetransformcontrol.cpp
    dmetransforminput.cpp
    dmetransformlist.cpp
    dmetransformoperator.cpp
    dmeunpackoperators.cpp
    dmeusersettings.cpp
    dmevertexdata.cpp
    dmmeshcomp.cpp
    dmobjserializer.cpp
    exportfacialanimation.cpp
    movieobjects_interfaces.cpp
)

add_library(Movieobjects STATIC ${MOVIEOBJECTS_SOURCES})

valve_apply_base_settings(Movieobjects)

target_compile_definitions(Movieobjects PRIVATE
    _LIB
    LIBNAME=movieobjects
    MOVIEOBJECTS_LIB
)

valve_set_static_lib_output(Movieobjects)
