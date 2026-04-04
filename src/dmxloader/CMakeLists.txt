# =============================================================================
# Dmxloader
# Generated from: dmxloader.vcxproj
# =============================================================================

set(DMXLOADER_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmxattribute.cpp
    dmxelement.cpp
    dmxloader.cpp
    dmxloadertext.cpp
    dmxserializationdictionary.cpp
    utlsoacontainer_serialization.cpp
)

add_library(Dmxloader STATIC ${DMXLOADER_SOURCES})

valve_apply_base_settings(Dmxloader)

target_compile_definitions(Dmxloader PRIVATE
    _LIB
    LIBNAME=dmxloader
    DMXLOADER_LIB
)

valve_set_static_lib_output(Dmxloader)
