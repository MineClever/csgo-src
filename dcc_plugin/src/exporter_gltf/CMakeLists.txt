add_library(maya_export_gltf STATIC
    GltfExportSession.cpp
    GltfExportSession.h
    GltfExportTranslator.cpp
    GltfExportTranslator.h
    GltfSceneExporter.cpp
    GltfSceneExporter.h
)

target_link_libraries(maya_export_gltf PUBLIC maya_common_gltf)
maya_dmx_apply_common_target_settings(maya_export_gltf)
