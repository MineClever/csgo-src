# =============================================================================
# shaderlib
# Generated from: shaderlib.vcxproj
# =============================================================================

set(SHADERLIB_SOURCES
    BaseShader.cpp
    ShaderDLL.cpp
    shaderlib_cvar.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
)

add_library(shaderlib STATIC ${SHADERLIB_SOURCES})

valve_apply_base_settings(shaderlib)

target_include_directories(shaderlib PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/materialsystem
)

target_compile_definitions(shaderlib PRIVATE
    _LIB
    LIBNAME=shaderlib
    FAST_MATERIALVAR_ACCESS
)

valve_set_static_lib_output(shaderlib)
