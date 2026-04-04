# =============================================================================
# vgui_controls
# Generated from: vgui_controls.vcxproj
# =============================================================================

set(VGUI_CONTROLS_SOURCES
    ${SRC_DIR}/common/protobuf_helpers.cpp
    AnalogBar.cpp
    AnimatingImagePanel.cpp
    AnimationController.cpp
    BitmapImagePanel.cpp
    BuildFactoryHelper.cpp
    BuildGroup.cpp
    BuildModeDialog.cpp
    Button.cpp
    CheckButton.cpp
    CheckButtonList.cpp
    CircularProgressBar.cpp
    ComboBox.cpp
    consoledialog.cpp
    ControllerMap.cpp
    controls.cpp
    cvartogglecheckbutton.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    DirectorySelectDialog.cpp
    Divider.cpp
    EditablePanel.cpp
    ExpandButton.cpp
    FileOpenDialog.cpp
    FileOpenStateMachine.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    FocusNavGroup.cpp
    Frame.cpp
    GraphPanel.cpp
    HTML.cpp
    Image.cpp
    ImageList.cpp
    ImagePanel.cpp
    InputDialog.cpp
    KeyBindingHelpDialog.cpp
    KeyBoardEditorDialog.cpp
    KeyRepeat.cpp
    Label.cpp
    ListPanel.cpp
    ListViewPanel.cpp
    Menu.cpp
    MenuBar.cpp
    MenuButton.cpp
    MenuItem.cpp
    MessageBox.cpp
    MessageDialog.cpp
    Panel.cpp
    PanelListPanel.cpp
    PerforceFileExplorer.cpp
    PerforceFileList.cpp
    perforcefilelistframe.cpp
    ProgressBar.cpp
    ProgressBox.cpp
    PropertyDialog.cpp
    PropertyPage.cpp
    PropertySheet.cpp
    QueryBox.cpp
    RadioButton.cpp
    RichText.cpp
    RotatingProgressBar.cpp
    savedocumentquery.cpp
    ScalableImagePanel.cpp
    ScrollableEditablePanel.cpp
    ScrollBar.cpp
    ScrollBarSlider.cpp
    SectionedListPanel.cpp
    Slider.cpp
    Splitter.cpp
    subrectimage.cpp
    TextEntry.cpp
    TextImage.cpp
    ToggleButton.cpp
    Tooltip.cpp
    ToolWindow.cpp
    TreeView.cpp
    TreeViewListControl.cpp
    URLLabel.cpp
    ${SRC_DIR}/vgui2/src/vgui_key_translation.cpp
    WizardPanel.cpp
    WizardSubPanel.cpp
)

add_library(vgui_controls STATIC ${VGUI_CONTROLS_SOURCES})

valve_apply_base_settings(vgui_controls)

target_include_directories(vgui_controls PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/vgui2/vgui_controls/generated_proto
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
    ${SRC_DIR}/thirdparty
    ${SRC_DIR}/thirdparty/cef
)

target_compile_definitions(vgui_controls PRIVATE
    _LIB
    LIBNAME=vgui_controls
    PROTOBUF
)

valve_set_static_lib_output(vgui_controls)
