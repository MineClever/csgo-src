add_library(maya_dmx_export STATIC
    DmxExportAnimation.cpp
    DmxExportAnimation.h
    DmxExportDeformers.cpp
    DmxExportDeformers.h
    DmxExportInternals.h
    DmxExportMeshMaterial.cpp
    DmxExportMeshMaterial.h
    DmxExportTextModel.cpp
    DmxExportTextModel.h
    DmxExportTranslator.cpp
    DmxExportTranslator.h
)

target_link_libraries(maya_dmx_export PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_export)
