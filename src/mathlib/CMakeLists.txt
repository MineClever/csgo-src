# =============================================================================
# mathlib
# Generated from: mathlib.vcxproj
# =============================================================================

set(MATHLIB_SOURCES
    almostequal.cpp
    anorms.cpp
    box_buoyancy.cpp
    bumpvects.cpp
    camera.cpp
    capsule.cpp
    cholesky.cpp
    color_conversion.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    eigen.cpp
    expressioncalculator.cpp
    halton.cpp
    IceKey.cpp
    imagequant.cpp
    kdop.cpp
    lightdesc.cpp
    mathlib_base.cpp
    planefit.cpp
    polygon.cpp
    polyhedron.cpp
    powsse.cpp
    quantize.cpp
    randsse.cpp
    simdvectormatrix.cpp
    simplex.cpp
    sparse_convolution_noise.cpp
    sphere.cpp
    spherical.cpp
    sse.cpp
    sseconst.cpp
    ssenoise.cpp
    transform.cpp
    vmatrix.cpp
    volumeculler.cpp
)

add_library(mathlib STATIC ${MATHLIB_SOURCES})

valve_apply_base_settings(mathlib)

target_include_directories(mathlib PRIVATE
    ${SRC_DIR}/public/mathlib
)

target_compile_definitions(mathlib PRIVATE
    _LIB
    LIBNAME=mathlib
    MATHLIB_LIB
)

valve_set_static_lib_output(mathlib)

# =============================================================================
# mathlib_extended
# Generated from: mathlib_extended.vcxproj
# =============================================================================

set(MATHLIB_EXTENDED_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    disjoint_set_forest.cpp
    dynamictree.cpp
    eigen.cpp
    feagglomerator.cpp
    femodel.cpp
    femodelbuilder.cpp
    femodeldesc.cpp
    simdvectormatrix.cpp
    softbody.cpp
    softbodyenvironment.cpp
    svd.cpp
    transform.cpp
)

add_library(mathlib_extended STATIC ${MATHLIB_EXTENDED_SOURCES})

valve_apply_base_settings(mathlib_extended)

target_include_directories(mathlib_extended PRIVATE
    ${SRC_DIR}/public/mathlib
)

target_compile_definitions(mathlib_extended PRIVATE
    _LIB
    LIBNAME=mathlib_extended
    MATHLIB_EXTENDED_LIB
    SOURCE1
)

valve_set_static_lib_output(mathlib_extended)
