#include "AnimationCurveUtils.h"

#include "MayaCommandUtils.h"

#include <maya/MDGModifier.h>
#include <maya/MFn.h>
#include <maya/MPlugArray.h>

namespace dcc_animation
{

MStatus ClearAnimationCurve(const MPlug &plug)
{
    if (plug.isNull())
    {
        return MS::kSuccess;
    }

    MPlugArray sourceConnections;
    MStatus status;
    plug.connectedTo(sourceConnections, true, false, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int sourceIndex = 0; sourceIndex < sourceConnections.length(); ++sourceIndex)
    {
        const MPlug sourcePlug = sourceConnections[sourceIndex];
        if (sourcePlug.isNull() || !sourcePlug.node().hasFn(MFn::kAnimCurve))
        {
            continue;
        }

        MDGModifier disconnectModifier;
        status = disconnectModifier.disconnect(sourcePlug, plug);
        if (!status)
        {
            return MS::kFailure;
        }
        status = disconnectModifier.doIt();
        if (!status)
        {
            return MS::kFailure;
        }

        MDGModifier deleteModifier;
        status = deleteModifier.deleteNode(sourcePlug.node());
        if (!status)
        {
            return MS::kFailure;
        }
        status = deleteModifier.doIt();
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SetCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType,
    MTime::Unit timeUnit)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status = ClearAnimationCurve(plug);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnAnimCurve curveFn;
    curveFn.create(plug, curveType, nullptr, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        curveFn.addKey(
            MTime(times[keyIndex], timeUnit),
            values[keyIndex],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SetCurveKeysAuto(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MTime::Unit timeUnit)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status = ClearAnimationCurve(plug);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnAnimCurve curveFn;
    curveFn.create(plug, nullptr, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        curveFn.addKey(
            MTime(times[keyIndex], timeUnit),
            values[keyIndex],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus EnsureAnimationLayerCached(
    const MString &configuredLayerName,
    bool replaceExisting,
    bool additiveLayer,
    bool overrideLayer,
    AnimationLayerCache &cache,
    MString *resolvedLayerName)
{
    if (cache.initialized)
    {
        if (resolvedLayerName)
        {
            *resolvedLayerName = cache.layerName;
        }
        return cache.layerName.length() > 0 ? MS::kSuccess : MS::kFailure;
    }

    cache.initialized = true;
    MStatus status = maya_cmd::EnsureAnimationLayer(
        configuredLayerName,
        replaceExisting,
        additiveLayer,
        overrideLayer,
        &cache.layerName);
    if (!status)
    {
        cache.layerName.clear();
        return MS::kFailure;
    }

    if (resolvedLayerName)
    {
        *resolvedLayerName = cache.layerName;
    }

    return MS::kSuccess;
}

MStatus SetCurveKeysOnAnimationLayer(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    const MString &layerName,
    bool timesAreSeconds,
    bool keepAdditiveMode)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    return maya_cmd::SetKeyframesOnAnimationLayer(
        layerName,
        plug,
        times.data(),
        values.data(),
        times.size(),
        timesAreSeconds,
        keepAdditiveMode);
}

} // namespace dcc_animation
