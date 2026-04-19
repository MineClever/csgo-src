#include "AnimationSampleUtils.h"

#include "MayaCommandUtils.h"

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFn.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStringArray.h>

namespace dcc_animation
{

namespace
{

MStatus SampleLayerPlugValue(
    const MString &layerName,
    const MPlug &plug,
    double time,
    MTime::Unit timeUnit,
    double &value)
{
    value = 0.0;
    if (plug.isNull())
    {
        return MS::kFailure;
    }

    if (IsEmptyAnimationLayerName(layerName))
    {
        CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(time, timeUnit));
        value = plug.asDouble();
        return MS::kSuccess;
    }

    MStringArray curveNames;
    MStatus status = maya_cmd::FindAnimationLayerCurvesForPlug(layerName, plug, curveNames);
    if (!status)
    {
        return MS::kFailure;
    }

    if (curveNames.length() == 0)
    {
        CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(time, timeUnit));
        value = plug.asDouble();
        return MS::kSuccess;
    }

    MObject curveObject;
    const bool curveFound = maya_cmd::TryGetNodeByName(curveNames[0], curveObject);
    if (!curveFound || curveObject.isNull() || !curveObject.hasFn(MFn::kAnimCurve))
    {
        return MS::kFailure;
    }

    MFnAnimCurve curveFn(curveObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    value = curveFn.evaluate(MTime(time, timeUnit));
    return MS::kSuccess;
}

} // namespace

CurrentTimeGuard::CurrentTimeGuard()
    : previousTime_(MAnimControl::currentTime())
{
}

CurrentTimeGuard::~CurrentTimeGuard()
{
    MAnimControl::setCurrentTime(previousTime_);
}

bool IsEmptyAnimationLayerName(const MString &layerName)
{
    return layerName.length() == 0 || layerName == "None" || layerName == "none";
}

MStatus BuildSceneReferenceTranslationSamples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MVector> &translations)
{
    translations.clear();
    translations.reserve(times.size());
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug translateXPlug = targetNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateYPlug = targetNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateZPlug = targetNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    CurrentTimeGuard currentTimeGuard;
    for (double time : times)
    {
        MAnimControl::setCurrentTime(MTime(time, timeUnit));
        translations.emplace_back(
            translateXPlug.asDouble(),
            translateYPlug.asDouble(),
            translateZPlug.asDouble());
    }

    return MS::kSuccess;
}

MStatus BuildSceneReferenceQuaternionSamples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MQuaternion> &rotations)
{
    rotations.clear();
    rotations.reserve(times.size());
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform transformFn(targetPath, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    CurrentTimeGuard currentTimeGuard;
    for (double time : times)
    {
        MAnimControl::setCurrentTime(MTime(time, timeUnit));
        MEulerRotation currentEulerRotation;
        status = transformFn.getRotation(currentEulerRotation);
        if (!status)
        {
            return MS::kFailure;
        }

        rotations.push_back(currentEulerRotation.asQuaternion());
    }

    return MS::kSuccess;
}

MStatus BuildSceneLayerTranslationSamples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MVector> &translations)
{
    translations.clear();
    translations.reserve(times.size());
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug translateXPlug = targetNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateYPlug = targetNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateZPlug = targetNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (double time : times)
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        status = SampleLayerPlugValue(layerName, translateXPlug, time, timeUnit, x);
        if (!status)
        {
            return MS::kFailure;
        }
        status = SampleLayerPlugValue(layerName, translateYPlug, time, timeUnit, y);
        if (!status)
        {
            return MS::kFailure;
        }
        status = SampleLayerPlugValue(layerName, translateZPlug, time, timeUnit, z);
        if (!status)
        {
            return MS::kFailure;
        }

        translations.emplace_back(x, y, z);
    }

    return MS::kSuccess;
}

MStatus BuildSceneLayerQuaternionSamples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    MTime::Unit timeUnit,
    std::vector<MQuaternion> &rotations)
{
    rotations.clear();
    rotations.reserve(times.size());
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform transformFn(targetPath, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MEulerRotation currentEulerRotation;
    status = transformFn.getRotation(currentEulerRotation);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug rotateXPlug = targetNodeFn.findPlug("rotateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug rotateYPlug = targetNodeFn.findPlug("rotateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug rotateZPlug = targetNodeFn.findPlug("rotateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (double time : times)
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        status = SampleLayerPlugValue(layerName, rotateXPlug, time, timeUnit, x);
        if (!status)
        {
            return MS::kFailure;
        }
        status = SampleLayerPlugValue(layerName, rotateYPlug, time, timeUnit, y);
        if (!status)
        {
            return MS::kFailure;
        }
        status = SampleLayerPlugValue(layerName, rotateZPlug, time, timeUnit, z);
        if (!status)
        {
            return MS::kFailure;
        }

        MEulerRotation eulerRotation(x, y, z);
        eulerRotation.reorderIt(currentEulerRotation.order);
        rotations.push_back(eulerRotation.asQuaternion());
    }

    return MS::kSuccess;
}

} // namespace dcc_animation
