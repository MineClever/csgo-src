add_library(maya_plugin_gltf MODULE
    PluginMain.cpp
)

target_link_libraries(maya_plugin_gltf PRIVATE
    maya_common_gltf
    maya_import_gltf
    maya_export_gltf
)

maya_dmx_configure_plugin_target(maya_plugin_gltf maya_gltf)
