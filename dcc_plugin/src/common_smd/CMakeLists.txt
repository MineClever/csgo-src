add_library(maya_common_smd STATIC
    MayaSmdCommon.cpp
    MayaSmdCommon.h
    SimpleSmdDocument.cpp
    SimpleSmdDocument.h
)

target_link_libraries(maya_common_smd PUBLIC maya_common)
maya_dmx_apply_common_target_settings(maya_common_smd)
