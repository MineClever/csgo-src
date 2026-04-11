add_library(maya_smd MODULE
    PluginMain.cpp
)

target_link_libraries(maya_smd PRIVATE
    maya_smd_common
    maya_smd_import
    maya_smd_export
)

maya_dmx_configure_plugin_target(maya_smd maya_smd)
