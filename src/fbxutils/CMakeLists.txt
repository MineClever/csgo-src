# src/fbxutils
# Generated from: fbxutils.vpc, vpc_scripts\source_lib_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_lib_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\source_lib_win32_debug.vpc, vpc_scripts\source_lib_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc, vpc_scripts\fbx_base.vpc

if(NOT VALVE_FBX_AVAILABLE)
    message(STATUS "fbxutils: skipping (no FBX backend available)")
    return()
endif()

set(FBXUTILS_FBXUTILS_SOURCES
    ${SRC_DIR}/public/fbxsystem/fbxsystem.cpp
    ${SRC_DIR}/public/fbxutils/dmfbxserializer.h
    ${SRC_DIR}/public/fbxsystem/ifbxsystem.h
    ${SRC_DIR}/public/fbxsystem/fbxsystem.h
)

if(VALVE_FBX_BACKEND_SDK)
    list(APPEND FBXUTILS_FBXUTILS_SOURCES
        ${SRC_DIR}/fbxutils/dmfbxserializer.cpp
    )
elseif(VALVE_FBX_BACKEND_OPENFBX)
    list(APPEND FBXUTILS_FBXUTILS_SOURCES
        ${SRC_DIR}/fbxutils/dmfbxserializer_openfbx.cpp
    )
endif()

add_library(fbxutils_fbxutils STATIC ${FBXUTILS_FBXUTILS_SOURCES})
add_library(fbxutils ALIAS fbxutils_fbxutils)

valve_apply_base_settings(fbxutils_fbxutils)

target_include_directories(fbxutils_fbxutils PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(fbxutils_fbxutils PRIVATE
    _LIB
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    FBXUTILS_LIB
)

if(VALVE_FBX_BACKEND_SDK)
    target_link_libraries(fbxutils_fbxutils PRIVATE valve_fbx_sdk)
    target_compile_definitions(fbxutils_fbxutils PUBLIC VALVE_FBX_BACKEND_SDK)
elseif(VALVE_FBX_BACKEND_OPENFBX)
    target_link_libraries(fbxutils_fbxutils PRIVATE valve_openfbx)
    target_include_directories(fbxutils_fbxutils PRIVATE ${VALVE_OPENFBX_DIR}/src)
    target_compile_definitions(fbxutils_fbxutils PUBLIC VALVE_FBX_BACKEND_OPENFBX)
endif()

set_target_properties(fbxutils_fbxutils PROPERTIES OUTPUT_NAME "fbxutils")

valve_set_static_lib_output(fbxutils_fbxutils)
