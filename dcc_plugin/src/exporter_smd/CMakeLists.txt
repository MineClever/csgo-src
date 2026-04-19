add_library(maya_export_smd STATIC
    SmdSceneExporter.cpp
    SmdSceneExporter.h
    SmdExportSession.cpp
    SmdExportSession.h
    SmdExportTranslator.cpp
    SmdExportTranslator.h
)

target_link_libraries(maya_export_smd PUBLIC maya_common_smd)
maya_dmx_apply_common_target_settings(maya_export_smd)
