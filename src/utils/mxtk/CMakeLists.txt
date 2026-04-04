# =============================================================================
# Mxtoolkitwin32
# Generated from: mxtoolkitwin32.vcxproj
# =============================================================================

set(MXTOOLKITWIN32_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    mx.cpp
    mxbmp.cpp
    mxbutton.cpp
    mxcheckbox.cpp
    mxchoice.cpp
    mxchoosecolor.cpp
    mxfiledialog.cpp
    mxglwindow.cpp
    mxgroupbox.cpp
    mxlabel.cpp
    mxlineedit.cpp
    mxlistbox.cpp
    mxlistview.cpp
    mxmatsyswindow.cpp
    mxmenu.cpp
    mxmenubar.cpp
    mxmessagebox.cpp
    mxpath.cpp
    mxpcx.cpp
    mxpopupmenu.cpp
    mxprogressbar.cpp
    mxradiobutton.cpp
    mxscrollbar.cpp
    mxslider.cpp
    mxstring.cpp
    mxtab.cpp
    mxtga.cpp
    mxtooltip.cpp
    mxtreeview.cpp
    mxwidget.cpp
    mxwindow.cpp
)

add_library(Mxtoolkitwin32 STATIC ${MXTOOLKITWIN32_SOURCES})

valve_apply_base_settings(Mxtoolkitwin32)

target_include_directories(Mxtoolkitwin32 PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Mxtoolkitwin32 PRIVATE
    _LIB
    LIBNAME=mxtoolkitwin32
)

valve_set_static_lib_output(Mxtoolkitwin32)
