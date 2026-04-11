add_library(maya_smd_import STATIC
    SmdMeshImporter.cpp
    SmdMeshImporter.h
    SmdImportSession.cpp
    SmdImportSession.h
    SmdSceneImporter.cpp
    SmdSceneImporter.h
    SmdImportTranslator.cpp
    SmdImportTranslator.h
)

target_link_libraries(maya_smd_import PUBLIC maya_smd_common)
maya_dmx_apply_common_target_settings(maya_smd_import)
