add_library(maya_export_dmx STATIC
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
    DmxExportSession.cpp
    DmxExportSession.h
    DmxExportTranslator.cpp
    DmxExportTranslator.h
)

target_link_libraries(maya_export_dmx PUBLIC maya_common_dmx)
maya_dmx_apply_common_target_settings(maya_export_dmx)
