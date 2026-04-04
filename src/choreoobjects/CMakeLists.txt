# =============================================================================
# choreoobjects
# Generated from: choreoobjects.vcxproj
# =============================================================================

set(CHOREOOBJECTS_SOURCES
    ${SRC_DIR}/game/shared/choreoactor.cpp
    ${SRC_DIR}/game/shared/choreochannel.cpp
    ${SRC_DIR}/game/shared/choreoevent.cpp
    ${SRC_DIR}/game/shared/choreoscene.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    ${SRC_DIR}/game/shared/sceneimage.cpp
)

add_library(choreoobjects STATIC ${CHOREOOBJECTS_SOURCES})

valve_apply_base_settings(choreoobjects)

target_include_directories(choreoobjects PRIVATE
    ${SRC_DIR}/game/shared
    ${SRC_DIR}/utils/common
)

target_compile_definitions(choreoobjects PRIVATE
    _LIB
    LIBNAME=choreoobjects
)

valve_set_static_lib_output(choreoobjects)
