add_library(maya_dmx_import STATIC
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
    DmxImportUtils.cpp
    DmxImportUtils.h
    ImportSession.cpp
    ImportSession.h
    DmxImportTranslator.cpp
    DmxImportTranslator.h
)

target_link_libraries(maya_dmx_import PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_import)
