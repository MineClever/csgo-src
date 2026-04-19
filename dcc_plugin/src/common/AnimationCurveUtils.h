#pragma once

#include <vector>

#include <maya/MFnAnimCurve.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>

namespace dcc_animation
{

struct AnimationLayerCache
{
    bool initialized = false;
    MString layerName;
};

MStatus ClearAnimationCurve(const MPlug &plug);

MStatus SetCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType,
    MTime::Unit timeUnit);

MStatus SetCurveKeysAuto(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MTime::Unit timeUnit);

MStatus EnsureAnimationLayerCached(
    const MString &configuredLayerName,
    bool replaceExisting,
    bool additiveLayer,
    bool overrideLayer,
    AnimationLayerCache &cache,
    MString *resolvedLayerName = nullptr);

MStatus SetCurveKeysOnAnimationLayer(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    const MString &layerName,
    bool timesAreSeconds,
    bool keepAdditiveMode);

} // namespace dcc_animation
