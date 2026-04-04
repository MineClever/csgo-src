# =============================================================================
# bonesetup
# Generated from: bonesetup.vcxproj
# =============================================================================

set(BONESETUP_SOURCES
    bone_constraints.cpp
    bone_decode.cpp
    bone_ik.cpp
    bone_setup.cpp
    bone_utils.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
)

add_library(bonesetup STATIC ${BONESETUP_SOURCES})

valve_apply_base_settings(bonesetup)

target_compile_definitions(bonesetup PRIVATE
    _LIB
    LIBNAME=bonesetup
)

valve_set_static_lib_output(bonesetup)
