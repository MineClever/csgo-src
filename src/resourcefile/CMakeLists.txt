# =============================================================================
# resourcefile
# Generated from: resourcefile.vcxproj
# =============================================================================

set(RESOURCEFILE_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    resourcedictionary.cpp
    resourcefile.cpp
    resourceintrospection.cpp
    resourcestream.cpp
)

add_library(resourcefile STATIC ${RESOURCEFILE_SOURCES})

valve_apply_base_settings(resourcefile)

target_compile_definitions(resourcefile PRIVATE
    _LIB
    LIBNAME=resourcefile
)

valve_set_static_lib_output(resourcefile)
