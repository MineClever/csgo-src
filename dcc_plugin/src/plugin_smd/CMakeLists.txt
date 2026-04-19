add_library(maya_plugin_smd MODULE
    PluginMain.cpp
)

target_link_libraries(maya_plugin_smd PRIVATE
    maya_common_smd
    maya_import_smd
    maya_export_smd
)

maya_dmx_configure_plugin_target(maya_plugin_smd maya_smd)
