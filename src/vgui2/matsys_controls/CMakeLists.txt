# =============================================================================
# matsys_controls
# Generated from: matsys_controls.vcxproj
# =============================================================================

set(MATSYS_CONTROLS_SOURCES
    assetpicker.cpp
    baseassetpicker.cpp
    colorpickerpanel.cpp
    curveeditorpanel.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    gamefiletreeview.cpp
    manipulator.cpp
    matsyscontrols.cpp
    mdlpanel.cpp
    mdlpicker.cpp
    mdlsequencepicker.cpp
    picker.cpp
    potterywheelpanel.cpp
    proceduraltexturepanel.cpp
    QCGenerator.cpp
    sequencepicker.cpp
    sheetsequencepanel.cpp
    tgapicker.cpp
    tgapreviewpanel.cpp
    vmtpanel.cpp
    vmtpicker.cpp
    vmtpreviewpanel.cpp
    vtfpicker.cpp
    vtfpreviewpanel.cpp
)

add_library(matsys_controls STATIC ${MATSYS_CONTROLS_SOURCES})

valve_apply_base_settings(matsys_controls)

target_include_directories(matsys_controls PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(matsys_controls PRIVATE
    _LIB
    LIBNAME=matsys_controls
)

valve_set_static_lib_output(matsys_controls)
