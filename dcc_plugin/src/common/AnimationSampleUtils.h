#pragma once

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

namespace dcc_animation
{

class CurrentTimeGuard
{
public:
    CurrentTimeGuard();
    ~CurrentTimeGuard();

private:
    MTime previousTime_;
};

bool IsEmptyAnimationLayerName(const MString &layerName);

MStatus BuildSceneReferenceTranslationSamples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MVector> &translations);

MStatus BuildSceneReferenceQuaternionSamples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MQuaternion> &rotations);

MStatus BuildSceneLayerTranslationSamples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MVector> &translations);

MStatus BuildSceneLayerQuaternionSamples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MQuaternion> &rotations);

} // namespace dcc_animation
