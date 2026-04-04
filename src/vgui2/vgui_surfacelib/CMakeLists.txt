# =============================================================================
# vgui_surfacelib
# Generated from: vgui_surfacelib.vcxproj
# =============================================================================

set(VGUI_SURFACELIB_SOURCES
    BitmapFont.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    FontAmalgam.cpp
    FontEffects.cpp
    fontmanager.cpp
    fonttexturecache.cpp
    texturedictionary.cpp
    Win32Font.cpp
)

add_library(vgui_surfacelib STATIC ${VGUI_SURFACELIB_SOURCES})

valve_apply_base_settings(vgui_surfacelib)

target_include_directories(vgui_surfacelib PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(vgui_surfacelib PRIVATE
    _LIB
    LIBNAME=vgui_surfacelib
)

valve_set_static_lib_output(vgui_surfacelib)
