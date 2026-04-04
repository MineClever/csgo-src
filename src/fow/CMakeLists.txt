# =============================================================================
# FoWLib
# Generated from: fow.vcxproj
# =============================================================================

set(FOWLIB_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    fow.cpp
    fow_2dplane.cpp
    fow_horizontalslice.cpp
    fow_lineoccluder.cpp
    fow_radiusoccluder.cpp
    fow_trisoup.cpp
    fow_viewer.cpp
)

add_library(FoWLib STATIC ${FOWLIB_SOURCES})

valve_apply_base_settings(FoWLib)

target_include_directories(FoWLib PRIVATE
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/public/fow
)

target_compile_definitions(FoWLib PRIVATE
    _LIB
    LIBNAME=fow
)

valve_set_static_lib_output(FoWLib)
