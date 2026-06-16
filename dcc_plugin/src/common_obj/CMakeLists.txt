add_library(maya_common_obj STATIC
    MayaObjCommon.cpp
    MayaObjCommon.h
    SimpleObjDocument.cpp
    SimpleObjDocument.h
    rapidobj.hpp
)

target_link_libraries(maya_common_obj PUBLIC maya_common)
maya_dmx_apply_common_target_settings(maya_common_obj)
