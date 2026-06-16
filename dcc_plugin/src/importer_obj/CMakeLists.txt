add_library(maya_import_obj STATIC
    ObjImportSession.cpp
    ObjImportSession.h
    ObjImportTranslator.cpp
    ObjImportTranslator.h
    ObjSceneImporter.cpp
    ObjSceneImporter.h
)

target_link_libraries(maya_import_obj PUBLIC maya_common_obj)
maya_dmx_apply_common_target_settings(maya_import_obj)
