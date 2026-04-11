add_library(maya_smd_export STATIC
    SmdSceneExporter.cpp
    SmdSceneExporter.h
    SmdExportSession.cpp
    SmdExportSession.h
    SmdExportTranslator.cpp
    SmdExportTranslator.h
)

target_link_libraries(maya_smd_export PUBLIC maya_smd_common)
maya_dmx_apply_common_target_settings(maya_smd_export)
