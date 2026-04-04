# =============================================================================
# Dme_controls
# Generated from: dme_controls.vcxproj
# =============================================================================

set(DME_CONTROLS_SOURCES
    2DSlider.cpp
    AssetBuilder.cpp
    attributeassetpickerpanel.cpp
    AttributeBasePickerPanel.cpp
    AttributeBoolChoicePanel.cpp
    attributebooleanpanel.cpp
    AttributeColorPickerPanel.cpp
    attributedetailtypepickerpanel.cpp
    AttributeElementPanel.cpp
    AttributeElementPickerPanel.cpp
    AttributeFilePickerPanel.cpp
    AttributeIntChoicePanel.cpp
    AttributeInterpolatorChoicePanel.cpp
    AttributeMDLPickerPanel.cpp
    AttributeSequencePickerPanel.cpp
    attributeshaderpickerpanel.cpp
    attributesheetsequencepickerpanel.cpp
    AttributeSoundPickerPanel.cpp
    AttributeStringChoicePanel.cpp
    attributesurfacepropertypickerpanel.cpp
    AttributeTextEntry.cpp
    AttributeTextPanel.cpp
    AttributeWidgetFactory.cpp
    BaseAttributeChoicePanel.cpp
    BaseAttributeDoubleChoicePanel.cpp
    BaseAttributePanel.cpp
    ChannelGraphPanel.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    dmecombinationsystemeditorpanel.cpp
    dmecontrols.cpp
    dmedageditpanel.cpp
    dmedagrenderpanel.cpp
    dmelogeditpanel.cpp
    dmemdlpanel.cpp
    dmepanel.cpp
    dmepicker.cpp
    dmepresetgroupeditorpanel.cpp
    DmeSourceDCCFilePanel.cpp
    DmeSourceSkinPanel.cpp
    ElementPropertiesTree.cpp
    FileListManager.cpp
    filtercombobox.cpp
    particlesystempanel.cpp
    particlesystempropertiespanel.cpp
    sheeteditorpanel.cpp
    soundpicker.cpp
    soundrecordpanel.cpp
    attributeslider.cpp
    BaseAnimationSetEditor.cpp
    BaseAnimationSetEditorController.cpp
    BaseAnimSetAttributeSliderPanel.cpp
    BaseAnimSetControlGroupPanel.cpp
    BaseAnimSetPresetFaderPanel.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    presetpicker.cpp
)

add_library(Dme_controls STATIC ${DME_CONTROLS_SOURCES})

valve_apply_base_settings(Dme_controls)

target_include_directories(Dme_controls PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Dme_controls PRIVATE
    _LIB
    LIBNAME=dme_controls
    DMECONTROLS_LIB
)

valve_set_static_lib_output(Dme_controls)
