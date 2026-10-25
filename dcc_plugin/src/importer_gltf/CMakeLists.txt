add_library(maya_import_gltf STATIC
    GltfImportSession.cpp
    GltfImportSession.h
    GltfImportTranslator.cpp
    GltfImportTranslator.h
    GltfSceneImporter.cpp
    GltfSceneImporter.h
    GltfAccessorReader.cpp
    GltfAccessorReader.h
    GltfAnimationImporter.cpp
    GltfAnimationImporter.h
    GltfMaterialImporter.cpp
    GltfMaterialImporter.h
    GltfMeshBuilder.cpp
    GltfMeshBuilder.h
    GltfSkinningHelper.cpp
    GltfSkinningHelper.h
)

target_link_libraries(maya_import_gltf PUBLIC maya_common_gltf)
maya_dmx_apply_common_target_settings(maya_import_gltf)
