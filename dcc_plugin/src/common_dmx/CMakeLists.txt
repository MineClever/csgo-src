add_library(maya_serialization_dmx STATIC
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

maya_dmx_apply_common_target_settings(maya_serialization_dmx)

add_library(maya_common_dmx STATIC
    MayaDmxCommon.cpp
    MayaDmxCommon.h
)

target_link_libraries(maya_common_dmx PUBLIC maya_common maya_serialization_dmx)
maya_dmx_apply_common_target_settings(maya_common_dmx)
