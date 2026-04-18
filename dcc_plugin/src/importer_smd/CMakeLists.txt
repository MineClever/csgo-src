add_library(maya_smd_import STATIC
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
)

target_link_libraries(maya_smd_import PUBLIC maya_smd_common)
maya_dmx_apply_common_target_settings(maya_smd_import)
