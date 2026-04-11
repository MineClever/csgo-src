add_library(maya_smd_common STATIC
    MayaSmdCommon.cpp
    MayaSmdCommon.h
    SimpleSmdDocument.cpp
    SimpleSmdDocument.h
)

maya_dmx_apply_common_target_settings(maya_smd_common)
