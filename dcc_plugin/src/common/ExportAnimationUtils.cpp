#include "ExportAnimationUtils.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <maya/MAnimControl.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlugArray.h>

namespace dcc_animation_export
{
namespace
{
constexpr std::array<const char *, 3> kTranslateAttributeNames = {"translateX", "translateY", "translateZ"};
constexpr std::array<const char *, 3> kRotateAttributeNames = {"rotateX", "rotateY", "rotateZ"};
constexpr std::array<const char *, 3> kScaleAttributeNames = {"scaleX", "scaleY", "scaleZ"};

void AppendUniqueTime(std::vector<double> &times, double value)
{
    const auto it = std::lower_bound(times.begin(), times.end(), value);
    if (it != times.end() && std::abs(*it - value) < 1.0e-6)
    {
        return;
    }
    if (it != times.begin())
    {
        const auto previous = it - 1;
        if (std::abs(*previous - value) < 1.0e-6)
        {
            return;
        }
    }

    times.insert(it, value);
}

class CurrentTimeGuard
{
public:
    CurrentTimeGuard()
        : previousTime_(MAnimControl::currentTime())
    {
    }

    ~CurrentTimeGuard()
    {
        MAnimControl::setCurrentTime(previousTime_);
    }

private:
    MTime previousTime_;
};

void CollectAnimationCurvesRecursive(
    const MPlug &plug,
    std::unordered_set<std::string> &visitedPlugs,
    std::unordered_set<unsigned int> &curveNodes,
    std::vector<MObject> &curves)
{
    if (plug.isNull())
    {
        return;
    }

    const std::string plugKey = plug.name().asChar();
    if (!visitedPlugs.insert(plugKey).second)
    {
        return;
    }

    MPlugArray sourcePlugs;
    plug.connectedTo(sourcePlugs, true, false);
    for (unsigned int sourceIndex = 0; sourceIndex < sourcePlugs.length(); ++sourceIndex)
    {
        MStatus status;
        const MObject sourceNode = sourcePlugs[sourceIndex].node(&status);
        if (!status || sourceNode.isNull())
        {
            continue;
        }

        if (sourceNode.hasFn(MFn::kAnimCurve))
        {
            const unsigned int curveKey = MObjectHandle(sourceNode).hashCode();
            if (curveNodes.insert(curveKey).second)
            {
                curves.push_back(sourceNode);
            }
            continue;
        }

        MFnDependencyNode sourceNodeFn(sourceNode, &status);
        if (!status)
        {
            continue;
        }

        MPlugArray nodeConnections;
        sourceNodeFn.getConnections(nodeConnections);
        for (unsigned int connectionIndex = 0; connectionIndex < nodeConnections.length(); ++connectionIndex)
        {
            const MPlug connectedPlug = nodeConnections[connectionIndex];
            if (!connectedPlug.isDestination())
            {
                continue;
            }
            CollectAnimationCurvesRecursive(connectedPlug, visitedPlugs, curveNodes, curves);
        }
    }
}
}

std::vector<MObject> FindAnimationCurvesForPlug(const MPlug &plug)
{
    std::vector<MObject> curves;
    std::unordered_set<std::string> visitedPlugs;
    std::unordered_set<unsigned int> curveNodes;
    CollectAnimationCurvesRecursive(plug, visitedPlugs, curveNodes, curves);
    return curves;
}

const std::array<const char *, 3> &GetTransformAttributeNames(TransformChannelGroup group)
{
    switch (group)
    {
    case TransformChannelGroup::Translation:
        return kTranslateAttributeNames;
    case TransformChannelGroup::Rotation:
        return kRotateAttributeNames;
    case TransformChannelGroup::Scale:
        return kScaleAttributeNames;
    default:
        return kTranslateAttributeNames;
    }
}

void AppendCurveTimes(const std::vector<MObject> &curveObjects, std::vector<double> &times, MTime::Unit timeUnit)
{
    for (const MObject &curveObject : curveObjects)
    {
        if (curveObject.isNull())
        {
            continue;
        }

        MStatus status;
        MFnAnimCurve curveFn(curveObject, &status);
        if (!status)
        {
            continue;
        }

        const unsigned int keyCount = curveFn.numKeys(&status);
        if (!status)
        {
            continue;
        }

        for (unsigned int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
        {
            const MTime keyTime = curveFn.time(keyIndex, &status);
            if (!status)
            {
                break;
            }

            AppendUniqueTime(times, keyTime.as(timeUnit));
        }
    }
}

double EvaluateCurveOrValue(const std::vector<MObject> &curveObjects, const MPlug &plug, double timeValue, MTime::Unit timeUnit)
{
    if (!curveObjects.empty())
    {
        CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(timeValue, timeUnit));
        return plug.asDouble();
    }

    double value = 0.0;
    plug.getValue(value);
    return value;
}

bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples)
{
    MStatus status;
    for (size_t index = 0; index < attributeNames.size(); ++index)
    {
        samples[index].plug = nodeFn.findPlug(attributeNames[index], true, &status);
        if (!status)
        {
            return false;
        }

        samples[index].curves = FindAnimationCurvesForPlug(samples[index].plug);
    }

    return true;
}

bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples)
{
    return BuildChannelSampleSet(nodeFn, GetTransformAttributeNames(TransformChannelGroup::Translation), samples.translation) &&
        BuildChannelSampleSet(nodeFn, GetTransformAttributeNames(TransformChannelGroup::Rotation), samples.rotation);
}

void AppendSampleSetTimes(
    const std::array<ScalarChannelSample, 3> &samples,
    std::vector<double> &times,
    MTime::Unit timeUnit)
{
    for (const ScalarChannelSample &sample : samples)
    {
        AppendCurveTimes(sample.curves, times, timeUnit);
    }
}

void AppendTransformSampleTimes(
    const TransformSampleSet &samples,
    std::vector<double> &times,
    MTime::Unit timeUnit)
{
    AppendSampleSetTimes(samples.translation, times, timeUnit);
    AppendSampleSetTimes(samples.rotation, times, timeUnit);
}

std::array<double, 3> EvaluateSampleSetValues(
    const std::array<ScalarChannelSample, 3> &samples,
    double timeValue,
    MTime::Unit timeUnit)
{
    std::array<double, 3> values{};
    for (size_t index = 0; index < samples.size(); ++index)
    {
        values[index] = EvaluateCurveOrValue(samples[index].curves, samples[index].plug, timeValue, timeUnit);
    }

    return values;
}

} // namespace dcc_animation_export
