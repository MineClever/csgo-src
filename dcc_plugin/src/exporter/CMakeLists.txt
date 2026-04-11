add_library(maya_dmx_export STATIC
    DmxExportAnimation.cpp
    DmxExportAnimation.h
    DmxExportDag.cpp
    DmxExportDag.h
    DmxExportDeformers.cpp
    DmxExportDeformers.h
    DmxExportInternals.cpp
    DmxExportInternals.h
    DmxExportMesh.cpp
    DmxExportMesh.h
    DmxExportMeshMaterial.cpp
    DmxExportMeshMaterial.h
    ExportSession.cpp
    ExportSession.h
    DmxExportTranslator.cpp
    DmxExportTranslator.h
)

target_link_libraries(maya_dmx_export PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_export)
