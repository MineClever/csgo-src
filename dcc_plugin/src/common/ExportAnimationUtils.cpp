#include "ExportAnimationUtils.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <maya/MAnimControl.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDependencyGraph.h>
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

}

std::vector<MObject> FindAnimationCurvesForPlug(const MPlug &plug)
{
    std::vector<MObject> curves;
    std::unordered_set<unsigned int> curveNodes;
    if (plug.isNull())
    {
        return curves;
    }

    MStatus status;
    MPlug traversalPlug(plug);
    MItDependencyGraph iterator(
        traversalPlug,
        MFn::kAnimCurve,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return curves;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        const MObject curveObject = iterator.currentItem(&status);
        if (!status || curveObject.isNull() || !curveObject.hasFn(MFn::kAnimCurve))
        {
            status = MS::kSuccess;
            continue;
        }

        const unsigned int curveKey = MObjectHandle(curveObject).hashCode();
        if (curveNodes.insert(curveKey).second)
        {
            curves.push_back(curveObject);
        }
    }

    return curves;
}

std::vector<MObject> FindAnimationCurvesForPlug(const MPlug &plug, CurveCache *curveCache)
{
    if (!curveCache || plug.isNull())
    {
        return FindAnimationCurvesForPlug(plug);
    }

    const std::string plugKey = plug.name().asChar();
    const auto it = curveCache->find(plugKey);
    if (it != curveCache->end())
    {
        return it->second;
    }

    std::vector<MObject> curves = FindAnimationCurvesForPlug(plug);
    curveCache->emplace(plugKey, curves);
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

std::array<double, 3> EvaluateSampleSetValuesAtCurrentTime(const std::array<ScalarChannelSample, 3> &samples)
{
    std::array<double, 3> values{};
    for (size_t index = 0; index < samples.size(); ++index)
    {
        values[index] = samples[index].plug.asDouble();
    }
    return values;
}

bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples)
{
    return BuildChannelSampleSet(nodeFn, attributeNames, samples, nullptr);
}

bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples,
    CurveCache *curveCache)
{
    MStatus status;
    for (size_t index = 0; index < attributeNames.size(); ++index)
    {
        samples[index].plug = nodeFn.findPlug(attributeNames[index], true, &status);
        if (!status)
        {
            return false;
        }

        samples[index].curves = FindAnimationCurvesForPlug(samples[index].plug, curveCache);
    }

    return true;
}

bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples)
{
    return BuildTransformSampleSet(nodeFn, samples, nullptr);
}

bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples, CurveCache *curveCache)
{
    return BuildChannelSampleSet(
               nodeFn,
               GetTransformAttributeNames(TransformChannelGroup::Translation),
               samples.translation,
               curveCache) &&
        BuildChannelSampleSet(
               nodeFn,
               GetTransformAttributeNames(TransformChannelGroup::Rotation),
               samples.rotation,
               curveCache);
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
    bool hasCurves = false;
    for (const ScalarChannelSample &sample : samples)
    {
        if (!sample.curves.empty())
        {
            hasCurves = true;
            break;
        }
    }

    if (hasCurves)
    {
        CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(timeValue, timeUnit));
        return EvaluateSampleSetValuesAtCurrentTime(samples);
    }

    std::array<double, 3> values{};
    for (size_t index = 0; index < samples.size(); ++index)
    {
        samples[index].plug.getValue(values[index]);
    }

    return values;
}

} // namespace dcc_animation_export
