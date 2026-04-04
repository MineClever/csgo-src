# =============================================================================
# Toolutils
# Generated from: toolutils.vcxproj
# =============================================================================

set(TOOLUTILS_SOURCES
    attributeelementchoicelist.cpp
    basepropertiescontainer.cpp
    basestatusbar.cpp
    BaseToolSystem.cpp
    ConsolePage.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    miniviewport.cpp
    recentfilelist.cpp
    savewindowpositions.cpp
    ScriptEditorPanel.cpp
    tool_main.cpp
    tooleditmenubutton.cpp
    ToolFileMenuButton.cpp
    ToolHelpMenuButton.cpp
    toolmenubar.cpp
    ToolMenuButton.cpp
    ToolSwitchMenuButton.cpp
    ToolUI.cpp
    vgui_tools.cpp
)

add_library(Toolutils STATIC ${TOOLUTILS_SOURCES})

valve_apply_base_settings(Toolutils)

target_include_directories(Toolutils PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Toolutils PRIVATE
    _LIB
    LIBNAME=toolutils
)

valve_set_static_lib_output(Toolutils)
