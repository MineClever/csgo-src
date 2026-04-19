#pragma once

#include <array>
#include <vector>

#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>

namespace dcc_animation_export
{
enum class TransformChannelGroup
{
    Translation,
    Rotation,
    Scale,
};

struct ScalarChannelSample
{
    MPlug plug;
    MObject curve;
};

struct TransformSampleSet
{
    std::array<ScalarChannelSample, 3> translation;
    std::array<ScalarChannelSample, 3> rotation;
};

MObject FindAnimationCurveForPlug(const MPlug &plug);
const std::array<const char *, 3> &GetTransformAttributeNames(TransformChannelGroup group);
void AppendCurveTimes(const MObject &curveObject, std::vector<double> &times, MTime::Unit timeUnit);
double EvaluateCurveOrValue(const MObject &curveObject, const MPlug &plug, double timeValue, MTime::Unit timeUnit);
bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples);
bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples);
void AppendSampleSetTimes(
    const std::array<ScalarChannelSample, 3> &samples,
    std::vector<double> &times,
    MTime::Unit timeUnit);
void AppendTransformSampleTimes(
    const TransformSampleSet &samples,
    std::vector<double> &times,
    MTime::Unit timeUnit);
std::array<double, 3> EvaluateSampleSetValues(
    const std::array<ScalarChannelSample, 3> &samples,
    double timeValue,
    MTime::Unit timeUnit);

} // namespace dcc_animation_export
