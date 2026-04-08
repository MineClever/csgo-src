#include "DmxImportAnimation.h"
#include "DmxImportInternals.h"

#include <algorithm>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>

namespace dmx_import_impl
{

static const simple_dmx::Element *FindFirstLogLayer(const simple_dmx::Document &document, const simple_dmx::Element *logElement)
{
    if (!logElement)
    {
        return nullptr;
    }

    const std::vector<const simple_dmx::Element *> layers = FindAttributeElementArray(document, logElement, "layers");
    return layers.empty() ? nullptr : layers.front();
}

static MStatus SetCurveKeys(const MPlug &plug, const std::vector<double> &times, const std::vector<double> &values, MFnAnimCurve::AnimCurveType curveType)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnAnimCurve curveFn;
    MObject curveObject = curveFn.create(plug, curveType, nullptr, &status);
    if (!status)
    {
        return status;
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
            return status;
        }
    }

    return MS::kSuccess;
}

static MStatus SetCurveKeysAuto(const MPlug &plug, const std::vector<double> &times, const std::vector<double> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnAnimCurve curveFn;
    MObject curveObject = curveFn.create(plug, nullptr, &status);
    if (!status)
    {
        return status;
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
            return status;
        }
    }

    return MS::kSuccess;
}

static MStatus ApplyVector3Animation(const MDagPath &targetPath, const simple_dmx::Element *logLayer)
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
    times.reserve(timeStrings.size());
    xValues.reserve(valueStrings.size());
    yValues.reserve(valueStrings.size());
    zValues.reserve(valueStrings.size());

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
        return status;
    }

    MPlug translateXPlug = targetNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return status;
    }
    MPlug translateYPlug = targetNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return status;
    }
    MPlug translateZPlug = targetNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return status;
    }

    status = SetCurveKeys(translateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return status;
    }
    status = SetCurveKeys(translateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return status;
    }
    return SetCurveKeys(translateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTL);
}

static MStatus ApplyQuaternionAnimation(const MDagPath &targetPath, const simple_dmx::Element *logLayer)
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
    times.reserve(timeStrings.size());
    xValues.reserve(valueStrings.size());
    yValues.reserve(valueStrings.size());
    zValues.reserve(valueStrings.size());

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
        return status;
    }

    MPlug rotateXPlug = targetNodeFn.findPlug("rotateX", true, &status);
    if (!status)
    {
        return status;
    }
    MPlug rotateYPlug = targetNodeFn.findPlug("rotateY", true, &status);
    if (!status)
    {
        return status;
    }
    MPlug rotateZPlug = targetNodeFn.findPlug("rotateZ", true, &status);
    if (!status)
    {
        return status;
    }

    status = SetCurveKeys(rotateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return status;
    }
    status = SetCurveKeys(rotateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return status;
    }
    return SetCurveKeys(rotateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTA);
}

static MStatus AddScalarAnimationTarget(std::vector<MPlug> &targets, const MObject &nodeObject, const std::string &attributeName)
{
    if (nodeObject.isNull() || attributeName.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return status;
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

static MStatus EnsureControlAttributeTargets(
    ImportContext &context,
    const std::string &targetName)
{
    if (targetName.empty() || context.importedControlPaths.empty())
    {
        return MS::kSuccess;
    }

    auto existingIt = context.importedScalarTargets.find(targetName);
    if (existingIt != context.importedScalarTargets.end() && !existingIt->second.empty())
    {
        return MS::kSuccess;
    }

    for (const MDagPath &controlPath : context.importedControlPaths)
    {
        MStatus status;
        MFnDependencyNode nodeFn(controlPath.node(), &status);
        if (!status)
        {
            return status;
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
                return status;
            }
            numericAttributeFn.setKeyable(true);
            numericAttributeFn.setStorable(true);
            numericAttributeFn.setReadable(true);
            numericAttributeFn.setWritable(true);
            status = nodeFn.addAttribute(attributeObject);
            if (!status)
            {
                return status;
            }
        }

        context.importedScalarTargets[targetName].push_back(ScalarAttributeBinding{controlPath.node(), targetName});
    }

    return MS::kSuccess;
}

static MStatus CollectFloatAnimationTargets(
    ImportContext &context,
    const simple_dmx::Element *targetElement,
    const std::string &attributeName,
    std::vector<MPlug> &targets)
{
    targets.clear();
    if (!targetElement || attributeName.empty())
    {
        return MS::kSuccess;
    }

    auto transformIt = context.importedTransformPaths.find(ElementKey(targetElement));
    if (transformIt != context.importedTransformPaths.end())
    {
        MStatus status = AddScalarAnimationTarget(targets, transformIt->second.node(), attributeName);
        if (!status)
        {
            return status;
        }
    }

    if (attributeName == "flexWeight")
    {
        auto blendShapeIt = context.importedBlendShapeTargets.find(targetElement->name);
        if (blendShapeIt != context.importedBlendShapeTargets.end())
        {
            for (const BlendShapeTargetBinding &binding : blendShapeIt->second)
            {
                MStatus status;
                MFnDependencyNode blendShapeNodeFn(binding.node, &status);
                if (!status)
                {
                    return status;
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

        MStatus status = EnsureControlAttributeTargets(context, targetElement->name);
        if (!status)
        {
            return status;
        }

        auto scalarIt = context.importedScalarTargets.find(targetElement->name);
        if (scalarIt != context.importedScalarTargets.end())
        {
            for (const ScalarAttributeBinding &binding : scalarIt->second)
            {
                status = AddScalarAnimationTarget(targets, binding.node, binding.attributeName);
                if (!status)
                {
                    return status;
                }
            }
        }
    }

    return MS::kSuccess;
}

static MStatus ApplyFloatAnimation(const MPlug &targetPlug, const simple_dmx::Element *logLayer)
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
    times.reserve(timeStrings.size());
    values.reserve(valueStrings.size());
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

    return SetCurveKeysAuto(targetPlug, times, values);
}

static MStatus ApplyFloatAnimation(const std::vector<MPlug> &targetPlugs, const simple_dmx::Element *logLayer)
{
    for (const MPlug &targetPlug : targetPlugs)
    {
        MStatus status = ApplyFloatAnimation(targetPlug, logLayer);
        if (!status)
        {
            return status;
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
    if (const simple_dmx::Element *animationList = FindAttributeElement(document, documentRoot, "animationList"))
    {
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(document, importRoot, "animationList"))
    {
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(document, modelRoot, "animationList"))
    {
        return animationList;
    }

    return nullptr;
}

const simple_dmx::Element *FindCombinationOperator(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot)
{
    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(document, documentRoot, "combinationOperator"))
    {
        return combinationOperator;
    }

    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(document, importRoot, "combinationOperator"))
    {
        return combinationOperator;
    }

    if (const simple_dmx::Element *combinationOperator = FindAttributeElement(document, modelRoot, "combinationOperator"))
    {
        return combinationOperator;
    }

    return nullptr;
}

MStatus ApplyChannelsClipAnimation(ImportContext &context, const simple_dmx::Element *channelsClip)
{
    if (!channelsClip)
    {
        return MS::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> channels = FindAttributeElementArray(context.document, channelsClip, "channels");
    for (const simple_dmx::Element *channel : channels)
    {
        if (!channel)
        {
            continue;
        }

        const std::string toAttribute = FindAttributeString(channel, "toAttribute");
        const simple_dmx::Element *toElement = FindAttributeElement(context.document, channel, "toElement");
        const simple_dmx::Element *logElement = FindAttributeElement(context.document, channel, "log");
        const simple_dmx::Element *logLayer = FindFirstLogLayer(context.document, logElement);
        if (!toElement || !logLayer)
        {
            continue;
        }

        MStatus status = MS::kSuccess;
        auto targetIt = context.importedTransformPaths.find(ElementKey(toElement));
        if (targetIt != context.importedTransformPaths.end() && toAttribute == "position")
        {
            status = ApplyVector3Animation(targetIt->second, logLayer);
        }
        else if (targetIt != context.importedTransformPaths.end() && toAttribute == "orientation")
        {
            status = ApplyQuaternionAnimation(targetIt->second, logLayer);
        }
        else if (logElement->type == "DmeFloatLog")
        {
            std::vector<MPlug> targetPlugs;
            status = CollectFloatAnimationTargets(context, toElement, toAttribute, targetPlugs);
            if (!status)
            {
                return status;
            }
            if (!targetPlugs.empty())
            {
                status = ApplyFloatAnimation(targetPlugs, logLayer);
            }
        }

        if (!status)
        {
            return status;
        }
    }

    const simple_dmx::Element *timeFrame = FindAttributeElement(context.document, channelsClip, "timeFrame");
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

MStatus CreateCombinationControls(
    ImportContext &context,
    const simple_dmx::Element *combinationOperator,
    const MObject &sceneRoot)
{
    if (!combinationOperator)
    {
        return MS::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> controls = FindAttributeElementArray(context.document, combinationOperator, "controls");
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
        return status;
    }

    std::string controlNodeName = combinationOperator->name.empty()
        ? "combinationControls"
        : SanitizeNodeName(combinationOperator->name + std::string("_controls"));
    controlNodeFn.setName(controlNodeName.c_str());

    MFnDependencyNode controlDependencyNode(controlNodeObject, &status);
    if (!status)
    {
        return status;
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
            return status;
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
            return status;
        }

        MPlug controlPlug = controlDependencyNode.findPlug(controlName.c_str(), true, &status);
        if (!status)
        {
            return status;
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
            return status;
        }

        ScalarAttributeBinding binding{controlNodeObject, controlName};
        context.importedScalarTargets[control->name].push_back(binding);
        for (const std::string &rawControlName : FindAttributeStringArray(control, "rawControlNames"))
        {
            context.importedScalarTargets[rawControlName].push_back(binding);
        }
    }

    return MS::kSuccess;
}

} // namespace dmx_import_impl
