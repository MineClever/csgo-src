#include "DmxExportAnimation.h"
#include "DmxExportInternals.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MFnAnimCurve.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MEulerRotation.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>

namespace dmx_export_impl
{

AnimationExporter::AnimationExporter(
    DocumentBuilder &builder,
    const std::vector<MDagPath> &exportRoots,
    ExportContext &context)
    : builder_(builder)
    , exportRoots_(exportRoots)
    , context_(context)
{
}

void AnimationExporter::bindCurrentDagContext(const MDagPath &dagPath)
{
    currentDagPath_ = dagPath;
    currentDagName_ = dagPath.partialPathName().asChar();

    const auto transformIt = context_.transformElementByPath.find(DagPathKey(currentDagPath_));
    currentTransformElement_ = transformIt != context_.transformElementByPath.end() ? transformIt->second : nullptr;
}

Element *AnimationExporter::findOrCreateFloatTargetElement(const std::string &targetName)
{
    auto targetIt = context_.floatTargetElementByName.find(targetName);
    if (targetIt != context_.floatTargetElementByName.end())
    {
        return targetIt->second;
    }

    Element *targetElement = builder_.CreateElement("DmElement", targetName);
    SetAttr(*targetElement, "flexWeight", ScalarAttr("float", "0.000000"));
    context_.floatTargetElementByName[targetName] = targetElement;
    return targetElement;
}

Element *AnimationExporter::buildFloatLog(
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<double> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatFloat(values[keyIndex]));
    }

    Element *logLayer = builder_.CreateElement("DmeFloatLogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("float_array", std::move(valueStrings)));

    Element *logElement = builder_.CreateElement("DmeFloatLog", logName);
    SetAttr(*logElement, "layers", builder_.ElementRefArray({logLayer}));
    return logElement;
}

Element *AnimationExporter::buildVector3Log(
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<std::array<double, 3>> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatVector3(values[keyIndex][0], values[keyIndex][1], values[keyIndex][2]));
    }

    Element *logLayer = builder_.CreateElement("DmeVector3LogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("vector3_array", std::move(valueStrings)));

    Element *logElement = builder_.CreateElement("DmeVector3Log", logName);
    SetAttr(*logElement, "layers", builder_.ElementRefArray({logLayer}));
    return logElement;
}

Element *AnimationExporter::buildQuaternionLog(
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<MQuaternion> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatQuaternion(values[keyIndex].x, values[keyIndex].y, values[keyIndex].z, values[keyIndex].w));
    }

    Element *logLayer = builder_.CreateElement("DmeQuaternionLogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("quaternion_array", std::move(valueStrings)));

    Element *logElement = builder_.CreateElement("DmeQuaternionLog", logName);
    SetAttr(*logElement, "layers", builder_.ElementRefArray({logLayer}));
    return logElement;
}

Element *AnimationExporter::buildFloatChannel(
    const std::string &name,
    Element *targetElement,
    const std::string &attributeName,
    Element *logElement)
{
    if (!targetElement || !logElement)
    {
        return nullptr;
    }

    Element *channelElement = builder_.CreateElement("DmeChannel", name);
    SetAttr(*channelElement, "toElement", builder_.ElementRef(targetElement));
    SetAttr(*channelElement, "toAttribute", ScalarAttr("string", attributeName));
    SetAttr(*channelElement, "log", builder_.ElementRef(logElement));
    return channelElement;
}

void AnimationExporter::appendScalarAnimationChannel(
    const MPlug &plug,
    Element *targetElement,
    const std::string &attributeName,
    const std::string &channelName)
{
    if (plug.isNull() || !targetElement)
    {
        return;
    }

    const MObject curveObject = FindAnimationCurveForPlug(plug);
    if (curveObject.isNull())
    {
        return;
    }

    std::vector<double> times;
    AppendCurveTimes(curveObject, times);
    if (times.empty())
    {
        return;
    }

    std::vector<double> values;
    for (double timeSeconds : times)
    {
        values.push_back(EvaluateCurveOrValue(curveObject, plug, timeSeconds));
        clipDurationSeconds_ = std::max(clipDurationSeconds_, timeSeconds);
    }

    Element *logElement = buildFloatLog(channelName + "_log", times, values);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement = buildFloatChannel(channelName, targetElement, attributeName, logElement))
    {
        channels_.push_back(channelElement);
    }
}

void AnimationExporter::appendCurrentPositionAnimationChannels()
{
    if (!currentTransformElement_)
    {
        return;
    }

    MStatus status;
    MFnDependencyNode nodeFn(currentDagPath_.node(), &status);
    if (!status)
    {
        return;
    }

    MPlug txPlug = nodeFn.findPlug("translateX", true, &status);
    MObject txCurve = FindAnimationCurveForPlug(txPlug);
    MPlug tyPlug = nodeFn.findPlug("translateY", true, &status);
    MObject tyCurve = FindAnimationCurveForPlug(tyPlug);
    MPlug tzPlug = nodeFn.findPlug("translateZ", true, &status);
    MObject tzCurve = FindAnimationCurveForPlug(tzPlug);
    std::vector<double> positionTimes;
    AppendCurveTimes(txCurve, positionTimes);
    AppendCurveTimes(tyCurve, positionTimes);
    AppendCurveTimes(tzCurve, positionTimes);
    if (positionTimes.empty())
    {
        return;
    }

    std::vector<std::array<double, 3>> positionValues;
    for (double timeSeconds : positionTimes)
    {
        positionValues.push_back({
            EvaluateCurveOrValue(txCurve, txPlug, timeSeconds),
            EvaluateCurveOrValue(tyCurve, tyPlug, timeSeconds),
            EvaluateCurveOrValue(tzCurve, tzPlug, timeSeconds)});
        clipDurationSeconds_ = std::max(clipDurationSeconds_, timeSeconds);
    }

    Element *logElement = buildVector3Log(currentDagName_ + "_position", positionTimes, positionValues);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement = buildFloatChannel(currentDagName_ + "_position_channel", currentTransformElement_, "position", logElement))
    {
        channels_.push_back(channelElement);
    }
}

void AnimationExporter::appendCurrentRotationAnimationChannels()
{
    if (!currentTransformElement_)
    {
        return;
    }

    MStatus status;
    MFnDependencyNode nodeFn(currentDagPath_.node(), &status);
    if (!status)
    {
        return;
    }

    MPlug rxPlug = nodeFn.findPlug("rotateX", true, &status);
    MObject rxCurve = FindAnimationCurveForPlug(rxPlug);
    MPlug ryPlug = nodeFn.findPlug("rotateY", true, &status);
    MObject ryCurve = FindAnimationCurveForPlug(ryPlug);
    MPlug rzPlug = nodeFn.findPlug("rotateZ", true, &status);
    MObject rzCurve = FindAnimationCurveForPlug(rzPlug);
    std::vector<double> rotationTimes;
    AppendCurveTimes(rxCurve, rotationTimes);
    AppendCurveTimes(ryCurve, rotationTimes);
    AppendCurveTimes(rzCurve, rotationTimes);
    if (rotationTimes.empty())
    {
        return;
    }

    std::vector<MQuaternion> rotationValues;
    for (double timeSeconds : rotationTimes)
    {
        const double rx = EvaluateCurveOrValue(rxCurve, rxPlug, timeSeconds);
        const double ry = EvaluateCurveOrValue(ryCurve, ryPlug, timeSeconds);
        const double rz = EvaluateCurveOrValue(rzCurve, rzPlug, timeSeconds);
        rotationValues.push_back(MEulerRotation(rx, ry, rz).asQuaternion());
        clipDurationSeconds_ = std::max(clipDurationSeconds_, timeSeconds);
    }

    Element *logElement = buildQuaternionLog(currentDagName_ + "_orientation", rotationTimes, rotationValues);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement = buildFloatChannel(currentDagName_ + "_orientation_channel", currentTransformElement_, "orientation", logElement))
    {
        channels_.push_back(channelElement);
    }
}

void AnimationExporter::appendCurrentScaleAnimationChannels()
{
    if (!currentTransformElement_)
    {
        return;
    }

    MStatus status;
    MFnDependencyNode nodeFn(currentDagPath_.node(), &status);
    if (!status)
    {
        return;
    }

    appendScalarAnimationChannel(nodeFn.findPlug("scaleX", true, &status), currentTransformElement_, "scaleX", currentDagName_ + "_scaleX_channel");
    appendScalarAnimationChannel(nodeFn.findPlug("scaleY", true, &status), currentTransformElement_, "scaleY", currentDagName_ + "_scaleY_channel");
    appendScalarAnimationChannel(nodeFn.findPlug("scaleZ", true, &status), currentTransformElement_, "scaleZ", currentDagName_ + "_scaleZ_channel");
}

void AnimationExporter::appendTransformAnimationChannels(const MDagPath &dagPath)
{
    bindCurrentDagContext(dagPath);
    if (!currentTransformElement_)
    {
        return;
    }
    appendCurrentPositionAnimationChannels();
    appendCurrentRotationAnimationChannels();
    appendCurrentScaleAnimationChannels();
}

void AnimationExporter::appendControlAnimationChannels(const MDagPath &dagPath)
{
    MStatus status;
    MFnDependencyNode nodeFn(dagPath.node(), &status);
    if (!status)
    {
        return;
    }

    const std::string nodeName = nodeFn.name().asChar();
    if (nodeName.size() < 9 || nodeName.rfind("_controls") != nodeName.size() - 9)
    {
        return;
    }

    const unsigned int attributeCount = nodeFn.attributeCount(&status);
    if (!status)
    {
        return;
    }

    for (unsigned int attrIdx = 0; attrIdx < attributeCount; ++attrIdx)
    {
        MObject attrObj = nodeFn.attribute(attrIdx, &status);
        if (!status || attrObj.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        MFnAttribute attrFn(attrObj);
        if (!attrFn.isKeyable() || attrObj.hasFn(MFn::kCompoundAttribute) || attrFn.isArray() ||
            (!attrObj.hasFn(MFn::kNumericAttribute) && !attrObj.hasFn(MFn::kUnitAttribute) && !attrObj.hasFn(MFn::kEnumAttribute)))
        {
            continue;
        }

        const std::string attributeName = attrFn.name().asChar();
        if (attributeName == "translateX" || attributeName == "translateY" || attributeName == "translateZ" ||
            attributeName == "rotateX" || attributeName == "rotateY" || attributeName == "rotateZ" ||
            attributeName == "scaleX" || attributeName == "scaleY" || attributeName == "scaleZ" ||
            attributeName == "visibility" || exportedFlexTargets_.find(attributeName) != exportedFlexTargets_.end())
        {
            continue;
        }

        MPlug plug = nodeFn.findPlug(attributeName.c_str(), true, &status);
        if (!status || plug.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        Element *targetElement = findOrCreateFloatTargetElement(attributeName);
        const size_t beforeChannelCount = channels_.size();
        appendScalarAnimationChannel(plug, targetElement, "flexWeight", attributeName + "_flex_channel");
        if (channels_.size() != beforeChannelCount)
        {
            exportedFlexTargets_.insert(attributeName);
        }
    }
}

void AnimationExporter::appendBlendShapeAnimationChannels(const MDagPath &meshPath)
{
    MStatus status;
    MObject meshObjectCopy(meshPath.node());
    MItDependencyGraph iterator(
        meshObjectCopy,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        MObject blendShapeObject = iterator.currentItem(&status);
        if (!status || blendShapeObject.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
        if (!status || weightArrayPlug.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        const unsigned int weightCount = weightArrayPlug.numElements(&status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        for (unsigned int weightIdx = 0; weightIdx < weightCount; ++weightIdx)
        {
            MPlug weightPlug = weightArrayPlug.elementByPhysicalIndex(weightIdx, &status);
            if (!status || weightPlug.isNull())
            {
                status = MS::kSuccess;
                continue;
            }

            MString alias = blendShapeNodeFn.plugsAlias(weightPlug);
            if (alias.length() == 0)
            {
                continue;
            }

            const std::string aliasName = alias.asChar();
            if (exportedFlexTargets_.find(aliasName) != exportedFlexTargets_.end())
            {
                continue;
            }

            Element *targetElement = findOrCreateFloatTargetElement(aliasName);
            const size_t beforeChannelCount = channels_.size();
            appendScalarAnimationChannel(weightPlug, targetElement, "flexWeight", aliasName + "_flex_channel");
            if (channels_.size() != beforeChannelCount)
            {
                exportedFlexTargets_.insert(aliasName);
            }
        }
    }
}

void AnimationExporter::collectControlAnimationChannelsRecursive(const MDagPath &dagPath)
{
    if (!dagPath.isValid())
    {
        return;
    }

    appendControlAnimationChannels(dagPath);

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (!(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        collectControlAnimationChannelsRecursive(childPath);
    }
}

void AnimationExporter::appendAnimationChannelsRecursive(const MDagPath &dagPath)
{
    if (!dagPath.isValid())
    {
        return;
    }

    appendTransformAnimationChannels(dagPath);
    appendControlAnimationChannels(dagPath);

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (childObject.hasFn(MFn::kMesh))
        {
            MDagPath meshPath = dagPath;
            meshPath.push(childObject);
            MFnDagNode meshDagNode(meshPath, &status);
            if (status && !meshDagNode.isIntermediateObject())
            {
                appendBlendShapeAnimationChannels(meshPath);
            }
            status = MS::kSuccess;
            continue;
        }

        if (!(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        appendAnimationChannelsRecursive(childPath);
    }
}

Element *AnimationExporter::BuildAnimationListElement()
{
    for (const MDagPath &rootPath : exportRoots_)
    {
        collectControlAnimationChannelsRecursive(rootPath);
    }

    for (const MDagPath &rootPath : exportRoots_)
    {
        appendAnimationChannelsRecursive(rootPath);
    }

    if (channels_.empty())
    {
        return nullptr;
    }

    Element *timeFrameElement = builder_.CreateElement("DmeTimeFrame");
    SetAttr(*timeFrameElement, "duration", ScalarAttr("time", FormatTimeSeconds(clipDurationSeconds_)));
    SetAttr(*timeFrameElement, "frameRate", ScalarAttr("float", "30.0"));

    Element *clipElement = builder_.CreateElement("DmeChannelsClip", "maya_export_animation");
    SetAttr(*clipElement, "channels", builder_.ElementRefArray(channels_));
    SetAttr(*clipElement, "timeFrame", builder_.ElementRef(timeFrameElement));

    Element *animationListElement = builder_.CreateElement("DmeAnimationList", "animationList");
    SetAttr(*animationListElement, "animations", builder_.ElementRefArray({clipElement}));
    return animationListElement;
}

Element *BuildAnimationListElement(DocumentBuilder &builder, const std::vector<MDagPath> &exportRoots, ExportContext &context)
{
    AnimationExporter exporter(builder, exportRoots, context);
    return exporter.BuildAnimationListElement();
}

} // namespace dmx_export_impl
