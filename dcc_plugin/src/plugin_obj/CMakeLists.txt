add_library(maya_plugin_obj MODULE
    PluginMain.cpp
)

target_link_libraries(maya_plugin_obj PRIVATE
    maya_common_obj
    maya_import_obj
)

maya_dmx_configure_plugin_target(maya_plugin_obj maya_obj)
