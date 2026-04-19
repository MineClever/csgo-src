add_library(maya_import_smd STATIC
    SmdAnimationImporter.cpp
    SmdAnimationImporter.h
    SmdMeshImporter.cpp
    SmdMeshImporter.h
    SmdImportSession.cpp
    SmdImportSession.h
    SmdImportUtils.cpp
    SmdImportUtils.h
    SmdSceneImporter.cpp
    SmdSceneImporter.h
    SmdSourceDeltaProcessor.cpp
    SmdSourceDeltaProcessor.h
    SmdImportTranslator.cpp
    SmdImportTranslator.h
    VtaImportTranslator.cpp
    VtaImportTranslator.h
    VtaSceneImporter.cpp
    VtaSceneImporter.h
)

target_link_libraries(maya_import_smd PUBLIC maya_common_smd)
maya_dmx_apply_common_target_settings(maya_import_smd)
