add_library(maya_dmx_import STATIC
    DmxImportAnimation.cpp
    DmxImportAnimation.h
    DmxImportDeformers.cpp
    DmxImportDeformers.h
    DmxImportInternals.h
    DmxImportMeshMaterial.cpp
    DmxImportMeshMaterial.h
    DmxImportUtils.cpp
    DmxImportUtils.h
    DmxImportTranslator.cpp
    DmxImportTranslator.h
)

target_link_libraries(maya_dmx_import PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_import)
