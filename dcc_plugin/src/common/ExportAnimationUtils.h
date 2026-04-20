#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>

namespace dcc_animation_export
{
using CurveCache = std::unordered_map<std::string, std::vector<MObject>>;

enum class TransformChannelGroup
{
    Translation,
    Rotation,
    Scale,
};

struct ScalarChannelSample
{
    MPlug plug;
    std::vector<MObject> curves;
};

struct TransformSampleSet
{
    std::array<ScalarChannelSample, 3> translation;
    std::array<ScalarChannelSample, 3> rotation;
};

std::vector<MObject> FindAnimationCurvesForPlug(const MPlug &plug);
std::vector<MObject> FindAnimationCurvesForPlug(const MPlug &plug, CurveCache *curveCache);
const std::array<const char *, 3> &GetTransformAttributeNames(TransformChannelGroup group);
void AppendCurveTimes(const std::vector<MObject> &curveObjects, std::vector<double> &times, MTime::Unit timeUnit);
double EvaluateCurveOrValue(const std::vector<MObject> &curveObjects, const MPlug &plug, double timeValue, MTime::Unit timeUnit);
bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples);
bool BuildChannelSampleSet(
    MFnDependencyNode &nodeFn,
    const std::array<const char *, 3> &attributeNames,
    std::array<ScalarChannelSample, 3> &samples,
    CurveCache *curveCache);
bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples);
bool BuildTransformSampleSet(MFnDependencyNode &nodeFn, TransformSampleSet &samples, CurveCache *curveCache);
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
std::array<double, 3> EvaluateSampleSetValuesAtCurrentTime(
    const std::array<ScalarChannelSample, 3> &samples);

} // namespace dcc_animation_export
