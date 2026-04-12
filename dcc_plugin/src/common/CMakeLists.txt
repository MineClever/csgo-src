add_library(maya_dmx_serialization STATIC
    SimpleDmxBinary.cpp
    SimpleDmxBinary.h
    SimpleDmxDocument.cpp
    SimpleDmxDocument.h
    SimpleDmxText.cpp
    SimpleDmxText.h
    SimpleDmxTypes.cpp
    SimpleDmxTypes.h
    SimpleDmxWrite.cpp
    SimpleDmxWrite.h
)

maya_dmx_apply_common_target_settings(maya_dmx_serialization)

add_library(maya_dmx_common STATIC
    ImportTransformCorrection.cpp
    ImportTransformCorrection.h
    MayaCommandUtils.cpp
    MayaCommandUtils.h
    MayaDmxCommon.cpp
    MayaDmxCommon.h
)

target_link_libraries(maya_dmx_common PUBLIC maya_dmx_serialization)
maya_dmx_apply_common_target_settings(maya_dmx_common)
