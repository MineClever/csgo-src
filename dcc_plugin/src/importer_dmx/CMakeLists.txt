add_library(maya_import_dmx STATIC
    DmxImportAnimation.cpp
    DmxImportAnimation.h
    DmxImportDag.cpp
    DmxImportDag.h
    DmxImportDeformers.cpp
    DmxImportDeformers.h
    DmxImportInternals.cpp
    DmxImportInternals.h
    DmxImportMesh.cpp
    DmxImportMesh.h
    DmxImportMeshMaterial.cpp
    DmxImportMeshMaterial.h
    DmxImportSession.cpp
    DmxImportSession.h
    DmxImportTranslator.cpp
    DmxImportTranslator.h
)

target_link_libraries(maya_import_dmx PUBLIC maya_common_dmx)
maya_dmx_apply_common_target_settings(maya_import_dmx)
