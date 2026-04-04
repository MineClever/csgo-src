# =============================================================================
# meshutils
# Generated from: meshutils.vcxproj
# =============================================================================

set(MESHUTILS_SOURCES
    ${SRC_DIR}/public/interpolatortypes.cpp
    atlas.cpp
    clipmesh.cpp
    convexhull.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    mesh.cpp
    simplify.cpp
    uvparam.cpp
    vertexops.cpp
)

add_library(meshutils STATIC ${MESHUTILS_SOURCES})

valve_apply_base_settings(meshutils)

target_include_directories(meshutils PRIVATE
    ${SRC_DIR}/public/meshutils
)

target_compile_definitions(meshutils PRIVATE
    _LIB
    LIBNAME=meshutils
)

valve_set_static_lib_output(meshutils)
