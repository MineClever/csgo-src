# =============================================================================
# vtf
# Generated from: vtf.vcxproj
# =============================================================================

set(VTF_SOURCES
    convert_x360.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    s3tc_decode.cpp
    vtf.cpp
)

add_library(vtf STATIC ${VTF_SOURCES})

valve_apply_base_settings(vtf)

target_include_directories(vtf PRIVATE
    ${SRC_DIR}/dx9sdk/include
)

target_compile_definitions(vtf PRIVATE
    _LIB
    LIBNAME=vtf
    fopen=dont_use_fopen
)

valve_set_static_lib_output(vtf)
