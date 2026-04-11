#include "DmxImportAnimation.h"
#include "DmxImportInternals.h"

#include <algorithm>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MQuaternion.h>
#include <maya/MTime.h>

namespace dmx_import_impl
{

namespace
{
bool ShouldSkipAppendTransformAnimation(const ImportContext &context, const simple_dmx::Element *targetElement)
{
    if (!targetElement || !dcc_import_policy::UsesAppendMissingObjects(context.scenePolicy))
    {
        return false;
    }

    return context.reusedTransformElementKeys.find(ElementKey(targetElement)) != context.reusedTransformElementKeys.end();
}
}


AnimationImporter::AnimationImporter(std::shared_ptr<ImportContext> context)
    : context_(context)
{
}

void AnimationImporter::setLookupRoots(
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot)
{
    documentRoot_ = documentRoot;
    importRoot_ = importRoot;
    modelRoot_ = modelRoot;
}

const simple_dmx::Element *AnimationImporter::findFirstLogLayer(const simple_dmx::Element *logElement) const
{
    if (!logElement)
    {
        return nullptr;
    }

    const std::vector<const simple_dmx::Element *> layers = FindAttributeElementArray(context_->document, logElement, "layers");
    return layers.empty() ? nullptr : layers.front();
}

MStatus AnimationImporter::setCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType) const
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnAnimCurve curveFn;
    curveFn.create(plug, curveType, nullptr, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        curveFn.addKey(
            MTime(times[keyIndex], MTime::kSeconds),
            values[keyIndex],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::setCurveKeysAuto(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values) const
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnAnimCurve curveFn;
    curveFn.create(plug, nullptr, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        curveFn.addKey(
            MTime(times[keyIndex], MTime::kSeconds),
            values[keyIndex],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::applyVector3Animation(const MDagPath &targetPath, const simple_dmx::Element *logLayer) const
{
    if (!logLayer)
    {
        return MS::kSuccess;
    }

    const std::vector<std::string> timeStrings = FindAttributeStringArray(logLayer, "times");
    const std::vector<std::string> valueStrings = FindAttributeStringArray(logLayer, "values");
    if (timeStrings.empty() || valueStrings.empty() || timeStrings.size() != valueStrings.size())
    {
        return MS::kSuccess;
    }

    std::vector<double> times;
    std::vector<double> xValues;
    std::vector<double> yValues;
    std::vector<double> zValues;
    for (size_t keyIndex = 0; keyIndex < timeStrings.size(); ++keyIndex)
    {
        const std::vector<double> timeValues = ParseNumberList(timeStrings[keyIndex]);
        const std::vector<double> vectorValues = ParseNumberList(valueStrings[keyIndex]);
        if (timeValues.empty() || vectorValues.size() < 3)
        {
            continue;
        }

        times.push_back(timeValues[0]);
        xValues.push_back(vectorValues[0]);
        yValues.push_back(vectorValues[1]);
        zValues.push_back(vectorValues[2]);
    }

    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug translateXPlug = targetNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug translateYPlug = targetNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug translateZPlug = targetNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = setCurveKeys(translateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setCurveKeys(translateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setCurveKeys(translateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTL);
}

MStatus AnimationImporter::applyQuaternionAnimation(const MDagPath &targetPath, const simple_dmx::Element *logLayer) const
{
    if (!logLayer)
    {
        return MS::kSuccess;
    }

    const std::vector<std::string> timeStrings = FindAttributeStringArray(logLayer, "times");
    const std::vector<std::string> valueStrings = FindAttributeStringArray(logLayer, "values");
    if (timeStrings.empty() || valueStrings.empty() || timeStrings.size() != valueStrings.size())
    {
        return MS::kSuccess;
    }

    std::vector<double> times;
    std::vector<double> xValues;
    std::vector<double> yValues;
    std::vector<double> zValues;
    for (size_t keyIndex = 0; keyIndex < timeStrings.size(); ++keyIndex)
    {
        const std::vector<double> timeValues = ParseNumberList(timeStrings[keyIndex]);
        const std::vector<double> quaternionValues = ParseNumberList(valueStrings[keyIndex]);
        if (timeValues.empty() || quaternionValues.size() < 4)
        {
            continue;
        }

        const MEulerRotation eulerRotation = MQuaternion(
            quaternionValues[0],
            quaternionValues[1],
            quaternionValues[2],
            quaternionValues[3]).asEulerRotation();

        times.push_back(timeValues[0]);
        xValues.push_back(eulerRotation.x);
        yValues.push_back(eulerRotation.y);
        zValues.push_back(eulerRotation.z);
    }

    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug rotateXPlug = targetNodeFn.findPlug("rotateX", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug rotateYPlug = targetNodeFn.findPlug("rotateY", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug rotateZPlug = targetNodeFn.findPlug("rotateZ", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = setCurveKeys(rotateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setCurveKeys(rotateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setCurveKeys(rotateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTA);
}

MStatus AnimationImporter::addScalarAnimationTarget(
    std::vector<MPlug> &targets,
    const MObject &nodeObject,
    const std::string &attributeName) const
{
    if (nodeObject.isNull() || attributeName.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug targetPlug = nodeFn.findPlug(attributeName.c_str(), true, &status);
    if (!status || targetPlug.isNull())
    {
        return MS::kSuccess;
    }

    const std::string plugName = targetPlug.name().asChar();
    for (const MPlug &existingPlug : targets)
    {
        if (plugName == existingPlug.name().asChar())
        {
            return MS::kSuccess;
        }
    }

    targets.push_back(targetPlug);
    return MS::kSuccess;
}

MStatus AnimationImporter::ensureControlAttributeTargets(const std::string &targetName)
{
    if (targetName.empty() || context_->importedControlPaths.empty())
    {
        return MS::kSuccess;
    }

    auto existingIt = context_->importedScalarTargets.find(targetName);
    if (existingIt != context_->importedScalarTargets.end() && !existingIt->second.empty())
    {
        return MS::kSuccess;
    }

    for (const MDagPath &controlPath : context_->importedControlPaths)
    {
        MStatus status;
        MFnDependencyNode nodeFn(controlPath.node(), &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MObject attributeObject = nodeFn.attribute(targetName.c_str(), &status);
        if (!status || attributeObject.isNull())
        {
            status = MS::kSuccess;
            MFnNumericAttribute numericAttributeFn;
            attributeObject = numericAttributeFn.create(
                targetName.c_str(),
                targetName.c_str(),
                MFnNumericData::kFloat,
                0.0f,
                &status);
            if (!status)
            {
                return MStatus::kFailure;
            }
            numericAttributeFn.setKeyable(true);
            numericAttributeFn.setStorable(true);
            numericAttributeFn.setReadable(true);
            numericAttributeFn.setWritable(true);
            status = nodeFn.addAttribute(attributeObject);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }

        context_->importedScalarTargets[targetName].push_back(ScalarAttributeBinding{controlPath.node(), targetName});
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::collectFloatAnimationTargets(
    const simple_dmx::Element *targetElement,
    const std::string &attributeName,
    std::vector<MPlug> &targets)
{
    targets.clear();
    if (!targetElement || attributeName.empty())
    {
        return MS::kSuccess;
    }

    auto transformIt = context_->importedTransformPaths.find(ElementKey(targetElement));
    if (transformIt != context_->importedTransformPaths.end())
    {
        MStatus status = addScalarAnimationTarget(targets, transformIt->second.node(), attributeName);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    if (attributeName == "flexWeight")
    {
        auto blendShapeIt = context_->importedBlendShapeTargets.find(targetElement->name);
        if (blendShapeIt != context_->importedBlendShapeTargets.end())
        {
            for (const BlendShapeTargetBinding &binding : blendShapeIt->second)
            {
                MStatus status;
                MFnDependencyNode blendShapeNodeFn(binding.node, &status);
                if (!status)
                {
                    return MStatus::kFailure;
                }

                MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
                if (!status || weightArrayPlug.isNull())
                {
                    continue;
                }

                MPlug targetPlug = weightArrayPlug.elementByLogicalIndex(binding.weightIndex, &status);
                if (!status || targetPlug.isNull())
                {
                    continue;
                }

                const std::string plugName = targetPlug.name().asChar();
                bool duplicate = false;
                for (const MPlug &existingPlug : targets)
                {
                    if (plugName == existingPlug.name().asChar())
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                {
                    targets.push_back(targetPlug);
                }
            }
        }

        MStatus status = ensureControlAttributeTargets(targetElement->name);
        if (!status)
        {
            return MStatus::kFailure;
        }

        auto scalarIt = context_->importedScalarTargets.find(targetElement->name);
        if (scalarIt != context_->importedScalarTargets.end())
        {
            for (const ScalarAttributeBinding &binding : scalarIt->second)
            {
                status = addScalarAnimationTarget(targets, binding.node, binding.attributeName);
                if (!status)
                {
                    return MStatus::kFailure;
                }
            }
        }
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::applyFloatAnimation(const MPlug &targetPlug, const simple_dmx::Element *logLayer) const
{
    if (!logLayer || targetPlug.isNull())
    {
        return MS::kSuccess;
    }

    const std::vector<std::string> timeStrings = FindAttributeStringArray(logLayer, "times");
    const std::vector<std::string> valueStrings = FindAttributeStringArray(logLayer, "values");
    if (timeStrings.empty() || valueStrings.empty() || timeStrings.size() != valueStrings.size())
    {
        return MS::kSuccess;
    }

    std::vector<double> times;
    std::vector<double> values;
    for (size_t keyIndex = 0; keyIndex < timeStrings.size(); ++keyIndex)
    {
        const std::vector<double> timeValues = ParseNumberList(timeStrings[keyIndex]);
        const std::vector<double> scalarValues = ParseNumberList(valueStrings[keyIndex]);
        if (timeValues.empty() || scalarValues.empty())
        {
            continue;
        }

        times.push_back(timeValues[0]);
        values.push_back(scalarValues[0]);
    }

    if (times.empty())
    {
        return MS::kSuccess;
    }

    return setCurveKeysAuto(targetPlug, times, values);
}

MStatus AnimationImporter::applyFloatAnimation(const std::vector<MPlug> &targetPlugs, const simple_dmx::Element *logLayer) const
{
    for (const MPlug &targetPlug : targetPlugs)
    {
        MStatus status = applyFloatAnimation(targetPlug, logLayer);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

const simple_dmx::Element *AnimationImporter::FindAnimationList() const
{
    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, documentRoot_, "animationList"))
    {
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, importRoot_, "animationList"))
    {
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, modelRoot_, "animationList"))
    {
        return animationList;
    }

    return nullptr;
}

const simple_dmx::Element *AnimationImporter::FindCombinationOperator() const
{
    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(context_->document, documentRoot_, "combinationOperator"))
    {
        return combinationOperator;
    }

    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(context_->document, importRoot_, "combinationOperator"))
    {
        return combinationOperator;
    }

    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(context_->document, modelRoot_, "combinationOperator"))
    {
        return combinationOperator;
    }

    return nullptr;
}

void AnimationImporter::bindCurrentChannel(const simple_dmx::Element *channel)
{
    currentChannel_ = channel;
    currentTargetElement_ = nullptr;
    currentLogElement_ = nullptr;
    currentLogLayer_ = nullptr;
    currentTargetAttribute_.clear();

    if (!currentChannel_)
    {
        return;
    }

    currentTargetAttribute_ = FindAttributeString(currentChannel_, "toAttribute");
    currentTargetElement_ = FindAttributeElement(context_->document, currentChannel_, "toElement");
    currentLogElement_ = FindAttributeElement(context_->document, currentChannel_, "log");
    currentLogLayer_ = findFirstLogLayer(currentLogElement_);
}

MStatus AnimationImporter::ApplyChannelsClipAnimation(const simple_dmx::Element *channelsClip)
{
    if (!channelsClip)
    {
        return MS::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> channels = FindAttributeElementArray(context_->document, channelsClip, "channels");
    for (const simple_dmx::Element *channel : channels)
    {
        if (!channel)
        {
            continue;
        }

        bindCurrentChannel(channel);
        if (!currentTargetElement_ || !currentLogLayer_)
        {
            continue;
        }

        MStatus status = MS::kSuccess;
        auto targetIt = context_->importedTransformPaths.find(ElementKey(currentTargetElement_));
        if (targetIt != context_->importedTransformPaths.end() && currentTargetAttribute_ == "position")
        {
            if (ShouldSkipAppendTransformAnimation(*context_, currentTargetElement_))
            {
                continue;
            }
            status = applyVector3Animation(targetIt->second, currentLogLayer_);
        }
        else if (targetIt != context_->importedTransformPaths.end() && currentTargetAttribute_ == "orientation")
        {
            if (ShouldSkipAppendTransformAnimation(*context_, currentTargetElement_))
            {
                continue;
            }
            status = applyQuaternionAnimation(targetIt->second, currentLogLayer_);
        }
        else if (currentLogElement_ && currentLogElement_->type == "DmeFloatLog")
        {
            std::vector<MPlug> targetPlugs;
            status = collectFloatAnimationTargets(currentTargetElement_, currentTargetAttribute_, targetPlugs);
            if (!status)
            {
                return MStatus::kFailure;
            }
            if (!targetPlugs.empty())
            {
                status = applyFloatAnimation(targetPlugs, currentLogLayer_);
            }
        }

        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    const simple_dmx::Element *timeFrame = FindAttributeElement(context_->document, channelsClip, "timeFrame");
    const std::vector<double> durationValues = ParseNumberList(FindAttributeString(timeFrame, "duration"));
    if (!durationValues.empty())
    {
        const MTime startTime(0.0, MTime::kSeconds);
        const MTime endTime(durationValues[0], MTime::kSeconds);
        MAnimControl::setMinTime(startTime);
        MAnimControl::setAnimationStartTime(startTime);
        MAnimControl::setMaxTime(endTime);
        MAnimControl::setAnimationEndTime(endTime);
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::CreateCombinationControls(
    const simple_dmx::Element *combinationOperator,
    const MObject &sceneRoot)
{
    if (!combinationOperator)
    {
        return MS::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> controls =
        FindAttributeElementArray(context_->document, combinationOperator, "controls");
    if (controls.empty())
    {
        return MS::kSuccess;
    }

    const std::vector<std::string> controlValueStrings = FindAttributeStringArray(combinationOperator, "controlValues");

    MStatus status;
    MFnTransform controlNodeFn;
    MObject controlNodeObject = controlNodeFn.create(sceneRoot, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    std::string controlNodeName = combinationOperator->name.empty()
        ? "combinationControls"
        : SanitizeNodeName(combinationOperator->name + std::string("_controls"));
    controlNodeFn.setName(controlNodeName.c_str());

    MFnDependencyNode controlDependencyNode(controlNodeObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
    {
        const simple_dmx::Element *control = controls[controlIndex];
        if (!control)
        {
            continue;
        }

        const std::string controlName = control->name.empty()
            ? std::string("control") + std::to_string(controlIndex)
            : SanitizeNodeName(control->name);

        MFnNumericAttribute numericAttributeFn;
        MObject attributeObject = numericAttributeFn.create(
            controlName.c_str(),
            controlName.c_str(),
            MFnNumericData::kFloat,
            0.0f,
            &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        numericAttributeFn.setKeyable(true);
        numericAttributeFn.setStorable(true);
        numericAttributeFn.setReadable(true);
        numericAttributeFn.setWritable(true);

        const std::vector<double> minValues = ParseNumberList(FindAttributeString(control, "flexMin"));
        if (!minValues.empty())
        {
            numericAttributeFn.setMin(static_cast<float>(minValues[0]));
        }

        const std::vector<double> maxValues = ParseNumberList(FindAttributeString(control, "flexMax"));
        if (!maxValues.empty())
        {
            numericAttributeFn.setMax(static_cast<float>(maxValues[0]));
        }

        status = controlDependencyNode.addAttribute(attributeObject);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MPlug controlPlug = controlDependencyNode.findPlug(controlName.c_str(), true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        float defaultValue = 0.0f;
        if (controlIndex < controlValueStrings.size())
        {
            const std::vector<double> controlValues = ParseNumberList(controlValueStrings[controlIndex]);
            if (!controlValues.empty())
            {
                defaultValue = static_cast<float>(controlValues.back());
            }
        }
        status = controlPlug.setFloat(defaultValue);
        if (!status)
        {
            return MStatus::kFailure;
        }

        ScalarAttributeBinding binding{controlNodeObject, controlName};
        context_->importedScalarTargets[control->name].push_back(binding);
        for (const std::string &rawControlName : FindAttributeStringArray(control, "rawControlNames"))
        {
            context_->importedScalarTargets[rawControlName].push_back(binding);
        }
    }

    return MS::kSuccess;
}

const simple_dmx::Element *FindAnimationList(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot)
{
    ImportContext proxyContext{document};
    auto contextPtr = std::shared_ptr<ImportContext>(&proxyContext, [](ImportContext *) {});
    AnimationImporter importer(contextPtr);
    importer.setLookupRoots(documentRoot, importRoot, modelRoot);
    return importer.FindAnimationList();
}

const simple_dmx::Element *FindCombinationOperator(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot)
{
    ImportContext proxyContext{document};
    auto contextPtr = std::shared_ptr<ImportContext>(&proxyContext, [](ImportContext *) {});
    AnimationImporter importer(contextPtr);
    importer.setLookupRoots(documentRoot, importRoot, modelRoot);
    return importer.FindCombinationOperator();
}

MStatus ApplyChannelsClipAnimation(ImportContext &context, const simple_dmx::Element *channelsClip)
{
    auto contextPtr = std::shared_ptr<ImportContext>(&context, [](ImportContext *) {});
    AnimationImporter importer(contextPtr);
    return importer.ApplyChannelsClipAnimation(channelsClip);
}

MStatus CreateCombinationControls(
    ImportContext &context,
    const simple_dmx::Element *combinationOperator,
    const MObject &sceneRoot)
{
    auto contextPtr = std::shared_ptr<ImportContext>(&context, [](ImportContext *) {});
    AnimationImporter importer(contextPtr);
    return importer.CreateCombinationControls(combinationOperator, sceneRoot);
}

} // namespace dmx_import_impl
