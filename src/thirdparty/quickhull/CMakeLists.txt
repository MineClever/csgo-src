# =============================================================================
# quickhull
# Generated from: quickhull.vcxproj
# =============================================================================

set(QUICKHULL_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    qhConvex.cpp
    qhHalfEdge.cpp
    qhMass.cpp
    qhMath.cpp
    qhMemory.cpp
)

add_library(quickhull STATIC ${QUICKHULL_SOURCES})

valve_apply_base_settings(quickhull)

target_include_directories(quickhull PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/public/quickhull
)

target_compile_definitions(quickhull PRIVATE
    _LIB
    LIBNAME=quickhull
)

valve_set_static_lib_output(quickhull)
