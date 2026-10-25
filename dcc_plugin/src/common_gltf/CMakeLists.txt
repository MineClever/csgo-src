add_library(maya_common_gltf STATIC
    MayaGltfCommon.cpp
    MayaGltfCommon.h
    SimpleGltfDocument.cpp
    SimpleGltfDocument.h
    cgltf.h
    cgltf_write.h
)

target_link_libraries(maya_common_gltf PUBLIC maya_common)
maya_dmx_apply_common_target_settings(maya_common_gltf)
