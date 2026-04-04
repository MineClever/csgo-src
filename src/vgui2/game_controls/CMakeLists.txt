# =============================================================================
# game_controls
# Generated from: game_controls.vcxproj
# =============================================================================

set(GAME_CONTROLS_SOURCES
    animdata.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dynamicrect.cpp
    gamegraphic.cpp
    GameLayer.cpp
    gamerect.cpp
    gamestage.cpp
    gametext.cpp
    gameuidefinition.cpp
    gameuidynamictextures.cpp
    gameuischeme.cpp
    GameUISystem.cpp
    gameuisystemmgr.cpp
    gameuisystemsurface.cpp
    graphicgroup.cpp
    hitarea.cpp
    inputgameui.cpp
    keyrepeat.cpp
    textdisplay.cpp
    typedlog.cpp
    uigeometry.cpp
    gameuiscript.cpp
    gameuiscriptinterface.cpp
    gameuiscriptsystem.cpp
    graphicscriptinterface.cpp
)

add_library(game_controls STATIC ${GAME_CONTROLS_SOURCES})

valve_apply_base_settings(game_controls)

target_include_directories(game_controls PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/public/game_controls
    ${SRC_DIR}/common/game_controls
)

target_compile_definitions(game_controls PRIVATE
    _LIB
    LIBNAME=game_controls
    GAMECONTROLS_LIB
)

valve_set_static_lib_output(game_controls)
