# =============================================================================
# studiorender
# Generated from: studiorender.vcxproj
# =============================================================================

set(STUDIORENDER_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    flexrenderdata.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    r_studio.cpp
    r_studiodecal.cpp
    r_studiodraw.cpp
    r_studiodraw_computeflexedvertex.cpp
    r_studioflex.cpp
    r_studiogettriangles.cpp
    r_studiolight.cpp
    r_studiosubd.cpp
    r_studiosubd_patches.cpp
    studiorender.cpp
    studiorendercontext.cpp
)

add_library(studiorender SHARED ${STUDIORENDER_SOURCES})

valve_apply_base_settings(studiorender)

target_compile_definitions(studiorender PRIVATE
    _USRDLL
    DLLNAME=studiorender
    MEMOVERRIDE_MODULE=studiorender
    STUDIORENDER_EXPORTS
    PROTECTED_THINGS_ENABLE
)

target_link_libraries(studiorender PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    bitmap
    mathlib
    vtf
    shaderlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(studiorender)

valve_publish_dll(studiorender "${GAME_BIN_DIR}")
