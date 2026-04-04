# =============================================================================
# Fgdlib
# Generated from: fgdlib.vcxproj
# =============================================================================

set(FGDLIB_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    gamedata.cpp
    gdclass.cpp
    gdvar.cpp
    inputoutput.cpp
    wckeyvalues.cpp
)

add_library(Fgdlib STATIC ${FGDLIB_SOURCES})

valve_apply_base_settings(Fgdlib)

target_include_directories(Fgdlib PRIVATE
    ${SRC_DIR}/utils/common
)

target_compile_definitions(Fgdlib PRIVATE
    _LIB
    LIBNAME=fgdlib
)

valve_set_static_lib_output(Fgdlib)
