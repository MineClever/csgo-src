# =============================================================================
# Materialobjects
# Generated from: materialobjects.vcxproj
# =============================================================================

set(MATERIALOBJECTS_SOURCES
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmeamalgtexture.cpp
    dmeimage.cpp
    dmeprecompiledtexture.cpp
    dmesheetsequence.cpp
    dmetexture.cpp
)

add_library(Materialobjects STATIC ${MATERIALOBJECTS_SOURCES})

valve_apply_base_settings(Materialobjects)

target_compile_definitions(Materialobjects PRIVATE
    _LIB
    LIBNAME=materialobjects
    MATERIALOBJECTS_LIB
)

valve_set_static_lib_output(Materialobjects)
