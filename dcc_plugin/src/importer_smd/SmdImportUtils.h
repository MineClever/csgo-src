#pragma once

#include <common/ImportTransformCorrection.h>
#include <common_smd/SimpleSmdDocument.h>

#include <string>

#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

namespace smd_import_impl
{

class CurrentTimeGuard
{
public:
    CurrentTimeGuard();
    ~CurrentTimeGuard();

private:
    MTime previousTime_;
};

bool IsEmptyLayerName(const std::string &layerName);
double ResolveCurrentFramesPerSecond();
MTime FrameIndexToTime(double frameIndex, double animationFps);
double FrameIndexToUiTimeValue(double frameIndex, double animationFps);
std::string SanitizeNodeName(std::string value);
MStatus SetPoseOnObject(
    MObject object,
    const simple_smd::SkeletonPose &pose,
    const dcc_import_transform::TransformCorrection *correction);

} // namespace smd_import_impl
