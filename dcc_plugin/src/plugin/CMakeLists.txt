add_library(maya_dmx MODULE
    PluginMain.cpp
)

target_link_libraries(maya_dmx PRIVATE
    maya_dmx_common
    maya_dmx_import
    maya_dmx_export
)

maya_dmx_configure_plugin_target(maya_dmx)
