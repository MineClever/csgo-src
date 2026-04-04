# =============================================================================
# Datamodel
# Generated from: datamodel.vcxproj
# =============================================================================

set(DATAMODEL_SOURCES
    clipboardmanager.cpp
    datamodel.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dependencygraph.cpp
    dmattribute.cpp
    dmconnect.cpp
    dmelement.cpp
    dmelementdictionary.cpp
    dmelementfactoryhelper.cpp
    DmElementFramework.cpp
    dmserializerbinary.cpp
    dmserializerkeyvalues.cpp
    dmserializerkeyvalues2.cpp
    undomanager.cpp
)

add_library(Datamodel STATIC ${DATAMODEL_SOURCES})

valve_apply_base_settings(Datamodel)

target_compile_definitions(Datamodel PRIVATE
    _LIB
    LIBNAME=datamodel
    DATAMODEL_LIB
)

valve_set_static_lib_output(Datamodel)
