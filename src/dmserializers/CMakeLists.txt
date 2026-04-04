# =============================================================================
# Dmserializers
# Generated from: dmserializers.vcxproj
# =============================================================================

set(DMSERIALIZERS_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmebaseimporter.cpp
    dmserializers.cpp
    importactbusy.cpp
    importcommentary.cpp
    importkeyvaluebase.cpp
    importmks.cpp
    importsfmv1.cpp
    importsfmv2.cpp
    importsfmv3.cpp
    importsfmv4.cpp
    importsfmv5.cpp
    importsfmv6.cpp
    importsfmv7.cpp
    importsfmv8.cpp
    importsfmv9.cpp
    importtex.cpp
    importvmf.cpp
    importvmt.cpp
)

add_library(Dmserializers STATIC ${DMSERIALIZERS_SOURCES})

valve_apply_base_settings(Dmserializers)

target_compile_definitions(Dmserializers PRIVATE
    _LIB
    LIBNAME=dmserializers
    DMSERIALIZERS_LIB
)

valve_set_static_lib_output(Dmserializers)
