add_library(maya_smd_common STATIC
    MayaSmdCommon.cpp
    MayaSmdCommon.h
    SimpleSmdDocument.cpp
    SimpleSmdDocument.h
)

target_link_libraries(maya_smd_common PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_smd_common)
