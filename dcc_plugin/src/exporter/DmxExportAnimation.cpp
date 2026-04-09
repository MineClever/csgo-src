#include "DmxExportAnimation.h"
#include "DmxExportInternals.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MEulerRotation.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

namespace dmx_export_impl
{

static Element *FindOrCreateFloatTargetElement(DocumentBuilder &builder, ExportContext &context, const std::string &targetName)
{
    auto targetIt = context.floatTargetElementByName.find(targetName);
    if (targetIt != context.floatTargetElementByName.end())
    {
        return targetIt->second;
    }

    Element *targetElement = builder.CreateElement("DmElement", targetName);
    SetAttr(*targetElement, "flexWeight", ScalarAttr("float", "0.000000"));
    context.floatTargetElementByName[targetName] = targetElement;
    return targetElement;
}

static Element *BuildFloatLog(
    DocumentBuilder &builder,
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
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatFloat(values[keyIndex]));
    }

    Element *logLayer = builder.CreateElement("DmeFloatLogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("float_array", std::move(valueStrings)));

    Element *logElement = builder.CreateElement("DmeFloatLog", logName);
    SetAttr(*logElement, "layers", builder.ElementRefArray({logLayer}));
    return logElement;
}

static Element *BuildVector3Log(
    DocumentBuilder &builder,
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
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatVector3(values[keyIndex][0], values[keyIndex][1], values[keyIndex][2]));
    }

    Element *logLayer = builder.CreateElement("DmeVector3LogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("vector3_array", std::move(valueStrings)));

    Element *logElement = builder.CreateElement("DmeVector3Log", logName);
    SetAttr(*logElement, "layers", builder.ElementRefArray({logLayer}));
    return logElement;
}

static Element *BuildQuaternionLog(
    DocumentBuilder &builder,
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
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatQuaternion(values[keyIndex].x, values[keyIndex].y, values[keyIndex].z, values[keyIndex].w));
    }

    Element *logLayer = builder.CreateElement("DmeQuaternionLogLayer", "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr("quaternion_array", std::move(valueStrings)));

    Element *logElement = builder.CreateElement("DmeQuaternionLog", logName);
    SetAttr(*logElement, "layers", builder.ElementRefArray({logLayer}));
    return logElement;
}

static Element *BuildFloatChannel(DocumentBuilder &builder, const std::string &name, Element *targetElement, const std::string &attributeName, Element *logElement)
{
    if (!targetElement || !logElement)
    {
        return nullptr;
    }

    Element *channelElement = builder.CreateElement("DmeChannel", name);
    SetAttr(*channelElement, "toElement", builder.ElementRef(targetElement));
    SetAttr(*channelElement, "toAttribute", ScalarAttr("string", attributeName));
    SetAttr(*channelElement, "log", builder.ElementRef(logElement));
    return channelElement;
}

static void AppendScalarAnimationChannel(
    DocumentBuilder &builder,
    const MPlug &plug,
    Element *targetElement,
    const std::string &attributeName,
    const std::string &channelName,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
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
    values.reserve(times.size());
    for (double timeSeconds : times)
    {
        values.push_back(EvaluateCurveOrValue(curveObject, plug, timeSeconds));
        clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
    }

    Element *logElement = BuildFloatLog(builder, channelName + "_log", times, values);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement = BuildFloatChannel(builder, channelName, targetElement, attributeName, logElement))
    {
        channels.push_back(channelElement);
    }
}

static void AppendTransformAnimationChannels(
    DocumentBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
{
    const auto transformIt = context.transformElementByPath.find(DagPathKey(dagPath));
    if (transformIt == context.transformElementByPath.end() || !transformIt->second)
    {
        return;
    }

    MStatus status;
    MFnDependencyNode nodeFn(dagPath.node(), &status);
    if (!status)
    {
        return;
    }

    const auto findPlug = [&](const char *name) -> MPlug {
        return nodeFn.findPlug(name, true, &status);
    };

    MPlug txPlug = findPlug("translateX");
    MObject txCurve = FindAnimationCurveForPlug(txPlug);
    MPlug tyPlug = findPlug("translateY");
    MObject tyCurve = FindAnimationCurveForPlug(tyPlug);
    MPlug tzPlug = findPlug("translateZ");
    MObject tzCurve = FindAnimationCurveForPlug(tzPlug);
    std::vector<double> positionTimes;
    AppendCurveTimes(txCurve, positionTimes);
    AppendCurveTimes(tyCurve, positionTimes);
    AppendCurveTimes(tzCurve, positionTimes);
    if (!positionTimes.empty())
    {
        std::vector<std::array<double, 3>> positionValues;
        positionValues.reserve(positionTimes.size());
        for (double timeSeconds : positionTimes)
        {
            positionValues.push_back({
                EvaluateCurveOrValue(txCurve, txPlug, timeSeconds),
                EvaluateCurveOrValue(tyCurve, tyPlug, timeSeconds),
                EvaluateCurveOrValue(tzCurve, tzPlug, timeSeconds)});
            clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
        }

        Element *logElement = BuildVector3Log(builder, std::string(dagPath.partialPathName().asChar()) + "_position", positionTimes, positionValues);
        if (logElement)
        {
            channels.push_back(BuildFloatChannel(builder, std::string(dagPath.partialPathName().asChar()) + "_position_channel", transformIt->second, "position", logElement));
        }
    }

    MPlug rxPlug = findPlug("rotateX");
    MObject rxCurve = FindAnimationCurveForPlug(rxPlug);
    MPlug ryPlug = findPlug("rotateY");
    MObject ryCurve = FindAnimationCurveForPlug(ryPlug);
    MPlug rzPlug = findPlug("rotateZ");
    MObject rzCurve = FindAnimationCurveForPlug(rzPlug);
    std::vector<double> rotationTimes;
    AppendCurveTimes(rxCurve, rotationTimes);
    AppendCurveTimes(ryCurve, rotationTimes);
    AppendCurveTimes(rzCurve, rotationTimes);
    if (!rotationTimes.empty())
    {
        std::vector<MQuaternion> rotationValues;
        rotationValues.reserve(rotationTimes.size());
        for (double timeSeconds : rotationTimes)
        {
            const double rx = EvaluateCurveOrValue(rxCurve, rxPlug, timeSeconds);
            const double ry = EvaluateCurveOrValue(ryCurve, ryPlug, timeSeconds);
            const double rz = EvaluateCurveOrValue(rzCurve, rzPlug, timeSeconds);
            rotationValues.push_back(MEulerRotation(rx, ry, rz).asQuaternion());
            clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
        }

        Element *logElement = BuildQuaternionLog(builder, std::string(dagPath.partialPathName().asChar()) + "_orientation", rotationTimes, rotationValues);
        if (logElement)
        {
            if (Element *channelElement = BuildFloatChannel(builder, std::string(dagPath.partialPathName().asChar()) + "_orientation_channel", transformIt->second, "orientation", logElement))
            {
                channels.push_back(channelElement);
            }
        }
    }

    AppendScalarAnimationChannel(builder, findPlug("scaleX"), transformIt->second, "scaleX", std::string(dagPath.partialPathName().asChar()) + "_scaleX_channel", channels, clipDurationSeconds);
    AppendScalarAnimationChannel(builder, findPlug("scaleY"), transformIt->second, "scaleY", std::string(dagPath.partialPathName().asChar()) + "_scaleY_channel", channels, clipDurationSeconds);
    AppendScalarAnimationChannel(builder, findPlug("scaleZ"), transformIt->second, "scaleZ", std::string(dagPath.partialPathName().asChar()) + "_scaleZ_channel", channels, clipDurationSeconds);
}

static void AppendControlAnimationChannels(
    DocumentBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
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
            attributeName == "visibility")
        {
            continue;
        }

        if (exportedFlexTargets.find(attributeName) != exportedFlexTargets.end())
        {
            continue;
        }

        MPlug plug = nodeFn.findPlug(attributeName.c_str(), true, &status);
        if (!status || plug.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        Element *targetElement = FindOrCreateFloatTargetElement(builder, context, attributeName);
        const size_t beforeChannelCount = channels.size();
        AppendScalarAnimationChannel(builder, plug, targetElement, "flexWeight", attributeName + "_flex_channel", channels, clipDurationSeconds);
        if (channels.size() != beforeChannelCount)
        {
            exportedFlexTargets.insert(attributeName);
        }
    }
}

static void AppendBlendShapeAnimationChannels(
    DocumentBuilder &builder,
    const MDagPath &meshPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
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
            if (exportedFlexTargets.find(aliasName) != exportedFlexTargets.end())
            {
                continue;
            }

            Element *targetElement = FindOrCreateFloatTargetElement(builder, context, aliasName);
            const size_t beforeChannelCount = channels.size();
            AppendScalarAnimationChannel(builder, weightPlug, targetElement, "flexWeight", aliasName + "_flex_channel", channels, clipDurationSeconds);
            if (channels.size() != beforeChannelCount)
            {
                exportedFlexTargets.insert(aliasName);
            }
        }
    }
}

static void CollectControlAnimationChannelsRecursive(
    DocumentBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
{
    if (!dagPath.isValid())
    {
        return;
    }

    AppendControlAnimationChannels(builder, dagPath, context, exportedFlexTargets, channels, clipDurationSeconds);

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
        CollectControlAnimationChannelsRecursive(builder, childPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }
}

static void AppendAnimationChannelsRecursive(
    DocumentBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<Element *> &channels,
    double &clipDurationSeconds)
{
    if (!dagPath.isValid())
    {
        return;
    }

    AppendTransformAnimationChannels(builder, dagPath, context, channels, clipDurationSeconds);
    AppendControlAnimationChannels(builder, dagPath, context, exportedFlexTargets, channels, clipDurationSeconds);

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
                AppendBlendShapeAnimationChannels(builder, meshPath, context, exportedFlexTargets, channels, clipDurationSeconds);
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
        AppendAnimationChannelsRecursive(builder, childPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }
}

Element *BuildAnimationListElement(DocumentBuilder &builder, const std::vector<MDagPath> &exportRoots, ExportContext &context)
{
    std::vector<Element *> channels;
    std::unordered_set<std::string> exportedFlexTargets;
    double clipDurationSeconds = 0.0;

    // Pre-pass: export _controls channels first so their flex targets take priority
    // over blendShape weight aliases with the same name.
    for (const MDagPath &rootPath : exportRoots)
    {
        CollectControlAnimationChannelsRecursive(builder, rootPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }

    // Main pass: transform channels + blendShape channels (controls already in exportedFlexTargets).
    for (const MDagPath &rootPath : exportRoots)
    {
        AppendAnimationChannelsRecursive(builder, rootPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }

    if (channels.empty())
    {
        return nullptr;
    }

    Element *timeFrameElement = builder.CreateElement("DmeTimeFrame");
    SetAttr(*timeFrameElement, "duration", ScalarAttr("time", FormatTimeSeconds(clipDurationSeconds)));
    SetAttr(*timeFrameElement, "frameRate", ScalarAttr("float", "30.0"));

    Element *clipElement = builder.CreateElement("DmeChannelsClip", "maya_export_animation");
    SetAttr(*clipElement, "channels", builder.ElementRefArray(channels));
    SetAttr(*clipElement, "timeFrame", builder.ElementRef(timeFrameElement));

    Element *animationListElement = builder.CreateElement("DmeAnimationList", "animationList");
    SetAttr(*animationListElement, "animations", builder.ElementRefArray({clipElement}));
    return animationListElement;
}

} // namespace dmx_export_impl
