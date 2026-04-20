add_library(maya_common STATIC
    AnimationCurveUtils.cpp
    AnimationCurveUtils.h
    AnimationSampleUtils.cpp
    AnimationSampleUtils.h
    ExportAnimationUtils.cpp
    ExportAnimationUtils.h
    TransformCorrection.cpp
    TransformCorrection.h
    MaterialExportUtils.cpp
    MaterialExportUtils.h
    MaterialUtils.cpp
    MaterialUtils.h
    MayaCommandUtils.cpp
    MayaCommandUtils.h
    SkinClusterUtils.cpp
    SkinClusterUtils.h
)

maya_dmx_apply_common_target_settings(maya_common)
