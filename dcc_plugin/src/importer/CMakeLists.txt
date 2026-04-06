add_library(maya_dmx_import STATIC
    DmxImportTranslator.cpp
    DmxImportTranslator.h
)

target_link_libraries(maya_dmx_import PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_import)
