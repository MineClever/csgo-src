add_library(maya_plugin_dmx MODULE
    PluginMain.cpp
)

target_link_libraries(maya_plugin_dmx PRIVATE
    maya_common_dmx
    maya_import_dmx
    maya_export_dmx
    maya_workflow_dmx
)

maya_dmx_configure_plugin_target(maya_plugin_dmx maya_dmx)
