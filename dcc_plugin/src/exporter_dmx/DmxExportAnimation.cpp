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
    std::shared_ptr<DocumentBuilder> builder,
    std::shared_ptr<const std::vector<MDagPath>> exportRoots,
    std::shared_ptr<ExportContext> context)
    : builder_(builder)
    , exportRoots_(exportRoots)
    , context_(context)
{
}

void AnimationExporter::bindCurrentDagContext(const MDagPath &dagPath)
{
    currentDagPath_ = dagPath;
    currentDagName_ = dagPath.partialPathName().asChar();

    const auto transformIt = context_->transformElementByPath.find(DagPathKey(currentDagPath_));
    currentTransformElement_ = transformIt != context_->transformElementByPath.end() ? transformIt->second : nullptr;
}

bool AnimationExporter::currentDagIsTopLevelNode() const
{
    return currentDagPath_.isValid() && context_->topLevelDagPaths.find(DagPathKey(currentDagPath_)) != context_->topLevelDagPaths.end();
}

Element *AnimationExporter::findOrCreateFloatTargetElement(const std::string &targetName)
{
    auto targetIt = context_->floatTargetElementByName.find(targetName);
    if (targetIt != context_->floatTargetElementByName.end())
    {
        return targetIt->second;
    }

    Element *targetElement = builder_->CreateElement("DmElement", targetName);
    SetAttr(*targetElement, "flexWeight", ScalarAttr("float", "0.000000"));
    context_->floatTargetElementByName[targetName] = targetElement;
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

    std::vector<std::string> valueStrings;
    valueStrings.reserve(values.size());
    for (double value : values)
    {
        valueStrings.push_back(FormatFloat(value));
    }

    return buildFormattedLog("DmeFloatLog", "DmeFloatLogLayer", "float_array", logName, times, std::move(valueStrings));
}

Element *AnimationExporter::buildFormattedLog(
    const std::string &logType,
    const std::string &layerType,
    const std::string &valueArrayType,
    const std::string &logName,
    const std::vector<double> &times,
    std::vector<std::string> valueStrings)
{
    if (times.empty() || valueStrings.empty() || times.size() != valueStrings.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    timeStrings.reserve(times.size());
    for (double timeValue : times)
    {
        timeStrings.push_back(FormatTimeSeconds(timeValue));
    }

    Element *logLayer = builder_->CreateElement(layerType, "base");
    SetAttr(*logLayer, "times", ScalarArrayAttr("time_array", std::move(timeStrings)));
    SetAttr(*logLayer, "values", ScalarArrayAttr(valueArrayType, std::move(valueStrings)));

    Element *logElement = builder_->CreateElement(logType, logName);
    SetAttr(*logElement, "layers", builder_->ElementRefArray({logLayer}));
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

    std::vector<std::string> valueStrings;
    valueStrings.reserve(values.size());
    for (const std::array<double, 3> &value : values)
    {
        valueStrings.push_back(FormatVector3(value[0], value[1], value[2]));
    }

    return buildFormattedLog("DmeVector3Log", "DmeVector3LogLayer", "vector3_array", logName, times, std::move(valueStrings));
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

    std::vector<std::string> valueStrings;
    valueStrings.reserve(values.size());
    for (const MQuaternion &value : values)
    {
        valueStrings.push_back(FormatQuaternion(value.x, value.y, value.z, value.w));
    }

    return buildFormattedLog(
        "DmeQuaternionLog",
        "DmeQuaternionLogLayer",
        "quaternion_array",
        logName,
        times,
        std::move(valueStrings));
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

    Element *channelElement = builder_->CreateElement("DmeChannel", name);
    SetAttr(*channelElement, "toElement", builder_->ElementRef(targetElement));
    SetAttr(*channelElement, "toAttribute", ScalarAttr("string", attributeName));
    SetAttr(*channelElement, "log", builder_->ElementRef(logElement));
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

    const std::vector<MObject> curveObjects = dcc_animation_export::FindAnimationCurvesForPlug(plug, &curveCache_);
    if (curveObjects.empty())
    {
        return;
    }

    std::vector<double> times;
    AppendCurveTimes(curveObjects, times);
    if (times.empty())
    {
        return;
    }

    std::vector<double> values;
    for (double timeSeconds : times)
    {
        values.push_back(EvaluateCurveOrValue(curveObjects, plug, timeSeconds));
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

void AnimationExporter::appendCurrentVectorTransformChannel(
    dcc_animation_export::TransformChannelGroup group,
    const std::string &targetAttributeName,
    const std::string &logNameSuffix,
    const std::string &channelNameSuffix)
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

    std::array<dcc_animation_export::ScalarChannelSample, 3> samples;
    if (!dcc_animation_export::BuildChannelSampleSet(
            nodeFn,
            dcc_animation_export::GetTransformAttributeNames(group),
            samples,
            &curveCache_))
    {
        return;
    }

    std::vector<double> times;
    dcc_animation_export::AppendSampleSetTimes(samples, times, MTime::kSeconds);
    if (times.empty())
    {
        return;
    }

    std::vector<std::array<double, 3>> values;
    const bool isTopLevelNode = currentDagIsTopLevelNode();
    for (double timeSeconds : times)
    {
        std::array<double, 3> transformedValues =
            dcc_animation_export::EvaluateSampleSetValues(samples, timeSeconds, MTime::kSeconds);
        if (isTopLevelNode)
        {
            const MVector correctedValues = dcc_export_transform::ApplyToTopLevelTranslation(
                context_->transformPolicy,
                MVector(transformedValues[0], transformedValues[1], transformedValues[2]));
            transformedValues = {correctedValues.x, correctedValues.y, correctedValues.z};
        }
        values.push_back(transformedValues);
        clipDurationSeconds_ = std::max(clipDurationSeconds_, timeSeconds);
    }

    Element *logElement = buildVector3Log(currentDagName_ + logNameSuffix, times, values);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement =
            buildFloatChannel(currentDagName_ + channelNameSuffix, currentTransformElement_, targetAttributeName, logElement))
    {
        channels_.push_back(channelElement);
    }
}

void AnimationExporter::appendCurrentQuaternionTransformChannel(
    dcc_animation_export::TransformChannelGroup group,
    const std::string &targetAttributeName,
    const std::string &logNameSuffix,
    const std::string &channelNameSuffix)
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

    std::array<dcc_animation_export::ScalarChannelSample, 3> samples;
    if (!dcc_animation_export::BuildChannelSampleSet(
            nodeFn,
            dcc_animation_export::GetTransformAttributeNames(group),
            samples,
            &curveCache_))
    {
        return;
    }

    std::vector<double> times;
    dcc_animation_export::AppendSampleSetTimes(samples, times, MTime::kSeconds);
    if (times.empty())
    {
        return;
    }

    std::vector<MQuaternion> values;
    const bool isTopLevelNode = currentDagIsTopLevelNode();
    for (double timeSeconds : times)
    {
        const std::array<double, 3> eulerValues =
            dcc_animation_export::EvaluateSampleSetValues(samples, timeSeconds, MTime::kSeconds);
        MQuaternion correctedRotation = MEulerRotation(eulerValues[0], eulerValues[1], eulerValues[2]).asQuaternion();
        if (isTopLevelNode)
        {
            correctedRotation = dcc_export_transform::ApplyToTopLevelQuaternion(
                context_->transformPolicy,
                correctedRotation);
        }
        values.push_back(correctedRotation);
        clipDurationSeconds_ = std::max(clipDurationSeconds_, timeSeconds);
    }

    Element *logElement = buildQuaternionLog(currentDagName_ + logNameSuffix, times, values);
    if (!logElement)
    {
        return;
    }

    if (Element *channelElement =
            buildFloatChannel(currentDagName_ + channelNameSuffix, currentTransformElement_, targetAttributeName, logElement))
    {
        channels_.push_back(channelElement);
    }
}

void AnimationExporter::appendCurrentPositionAnimationChannels()
{
    appendCurrentVectorTransformChannel(
        dcc_animation_export::TransformChannelGroup::Translation,
        "position",
        "_position",
        "_position_channel");
}

void AnimationExporter::appendCurrentRotationAnimationChannels()
{
    appendCurrentQuaternionTransformChannel(
        dcc_animation_export::TransformChannelGroup::Rotation,
        "orientation",
        "_orientation",
        "_orientation_channel");
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

    std::array<dcc_animation_export::ScalarChannelSample, 3> scaleSamples;
    const std::array<const char *, 3> &scaleAttributeNames =
        dcc_animation_export::GetTransformAttributeNames(dcc_animation_export::TransformChannelGroup::Scale);
    if (!dcc_animation_export::BuildChannelSampleSet(nodeFn, scaleAttributeNames, scaleSamples, &curveCache_))
    {
        return;
    }

    for (size_t index = 0; index < scaleSamples.size(); ++index)
    {
        const std::string attributeName = scaleAttributeNames[index];
        appendScalarAnimationChannel(
            scaleSamples[index].plug,
            currentTransformElement_,
            attributeName,
            currentDagName_ + "_" + attributeName + "_channel");
    }
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
    for (const MDagPath &rootPath : *exportRoots_)
    {
        collectControlAnimationChannelsRecursive(rootPath);
    }

    for (const MDagPath &rootPath : *exportRoots_)
    {
        appendAnimationChannelsRecursive(rootPath);
    }

    if (channels_.empty())
    {
        return nullptr;
    }

    Element *timeFrameElement = builder_->CreateElement("DmeTimeFrame");
    SetAttr(*timeFrameElement, "duration", ScalarAttr("time", FormatTimeSeconds(clipDurationSeconds_)));
    SetAttr(*timeFrameElement, "frameRate", ScalarAttr("float", "30.0"));

    Element *clipElement = builder_->CreateElement("DmeChannelsClip", "maya_export_animation");
    SetAttr(*clipElement, "channels", builder_->ElementRefArray(channels_));
    SetAttr(*clipElement, "timeFrame", builder_->ElementRef(timeFrameElement));

    Element *animationListElement = builder_->CreateElement("DmeAnimationList", "animationList");
    SetAttr(*animationListElement, "animations", builder_->ElementRefArray({clipElement}));
    return animationListElement;
}

Element *BuildAnimationListElement(DocumentBuilder &builder, const std::vector<MDagPath> &exportRoots, ExportContext &context)
{
    auto builderPtr = std::shared_ptr<DocumentBuilder>(&builder, [](DocumentBuilder *) {});
    auto exportRootsPtr = std::shared_ptr<const std::vector<MDagPath>>(&exportRoots, [](const std::vector<MDagPath> *) {});
    auto contextPtr = std::shared_ptr<ExportContext>(&context, [](ExportContext *) {});
    AnimationExporter exporter(builderPtr, exportRootsPtr, contextPtr);
    return exporter.BuildAnimationListElement();
}

} // namespace dmx_export_impl
