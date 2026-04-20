#include "DmxImportAnimation.h"
#include "DmxImportInternals.h"

#include <common/AnimationCurveUtils.h>
#include <common/AnimationSampleUtils.h>
#include <common/MayaCommandUtils.h>
#include <common/TransformCorrection.h>
#include <common/SourceDeltaUtils.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MPoint.h>
#include <maya/MQuaternion.h>
#include <maya/MTime.h>
#include <maya/MTransformationMatrix.h>

namespace dmx_import_impl
{

namespace detail
{

constexpr double kSourceDeltaRadiansToDegrees = 180.0 / 3.14159265358979323846;

dcc_import_policy::SourceDeltaMode ParseSourceDeltaModeValue(const std::string &value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "subtract")
    {
        return dcc_import_policy::SourceDeltaMode::Subtract;
    }
    if (normalized == "presubtract")
    {
        return dcc_import_policy::SourceDeltaMode::PreSubtract;
    }
    if (normalized == "lineardelta")
    {
        return dcc_import_policy::SourceDeltaMode::LinearDelta;
    }
    if (normalized == "splinedelta")
    {
        return dcc_import_policy::SourceDeltaMode::SplineDelta;
    }
    return dcc_import_policy::SourceDeltaMode::None;
}

} // namespace detail

using namespace detail;

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

AnimationImporter::SourceDeltaSettings AnimationImporter::resolveSourceDeltaSettings(const simple_dmx::Element *channelsClip) const
{
    SourceDeltaSettings settings;
    settings.mode = context_->scenePolicy.sourceDeltaMode;
    settings.useClip = context_->scenePolicy.sourceDeltaUseClip;
    settings.sceneClipName = context_->scenePolicy.sourceDeltaClip;
    settings.referenceFrame = std::max(0, context_->scenePolicy.sourceDeltaReferenceFrame);

    if (!channelsClip)
    {
        return settings;
    }

    const std::string modeValue = FindAttributeString(channelsClip, "sourceDeltaMode");
    if (!modeValue.empty())
    {
        settings.mode = ParseSourceDeltaModeValue(modeValue);
    }

    const std::string useClipValue = FindAttributeString(channelsClip, "sourceDeltaUseClip");
    if (!useClipValue.empty())
    {
        std::string normalizedUseClip = useClipValue;
        std::transform(normalizedUseClip.begin(), normalizedUseClip.end(), normalizedUseClip.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        settings.useClip = normalizedUseClip == "1" || normalizedUseClip == "true" || normalizedUseClip == "yes";
    }

    const std::string sceneClipValue = FindAttributeString(channelsClip, "sourceDeltaClip");
    if (!sceneClipValue.empty())
    {
        settings.sceneClipName = sceneClipValue;
    }

    const std::string referenceFrameValue = FindAttributeString(channelsClip, "sourceDeltaReferenceFrame");
    if (!referenceFrameValue.empty())
    {
        settings.referenceFrame = std::max(0, std::atoi(referenceFrameValue.c_str()));
    }

    if (settings.sceneClipName == "None" || settings.sceneClipName == "none")
    {
        settings.sceneClipName.clear();
    }

    return settings;
}

bool AnimationImporter::shouldImportChannelsClip(const simple_dmx::Element *channelsClip, const SourceDeltaSettings &settings) const
{
    if (!channelsClip)
    {
        return false;
    }

    if (settings.mode == dcc_import_policy::SourceDeltaMode::None)
    {
        return true;
    }

    return true;
}

MStatus AnimationImporter::extractVector3AnimationSamples(
    const MDagPath &targetPath,
    const simple_dmx::Element *logLayer,
    Vector3AnimationSamples &samples) const
{
    samples.times.clear();
    samples.values.clear();
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

    const bool applyTopLevelCorrection =
        isTopLevelImportedPath(targetPath) &&
        !context_->topLevelPreTransform.isEquivalent(MMatrix::identity);
    for (size_t keyIndex = 0; keyIndex < timeStrings.size(); ++keyIndex)
    {
        const std::vector<double> timeValues = ParseNumberList(timeStrings[keyIndex]);
        const std::vector<double> vectorValues = ParseNumberList(valueStrings[keyIndex]);
        if (timeValues.empty() || vectorValues.size() < 3)
        {
            continue;
        }

        MVector correctedValue(vectorValues[0], vectorValues[1], vectorValues[2]);
        if (applyTopLevelCorrection)
        {
            const MPoint transformed = MPoint(correctedValue) * context_->topLevelPreTransform;
            correctedValue = MVector(transformed.x, transformed.y, transformed.z);
        }

        samples.times.push_back(timeValues[0]);
        samples.values.push_back(correctedValue);
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::extractQuaternionAnimationSamples(
    const MDagPath &targetPath,
    const simple_dmx::Element *logLayer,
    QuaternionAnimationSamples &samples) const
{
    samples.times.clear();
    samples.values.clear();
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

    const bool applyTopLevelCorrection =
        isTopLevelImportedPath(targetPath) &&
        !context_->topLevelPreTransform.isEquivalent(MMatrix::identity);
    MQuaternion correctionRotation;
    if (applyTopLevelCorrection)
    {
        MTransformationMatrix correctionTransform(context_->topLevelPreTransform);
        correctionRotation = correctionTransform.rotation();
    }

    for (size_t keyIndex = 0; keyIndex < timeStrings.size(); ++keyIndex)
    {
        const std::vector<double> timeValues = ParseNumberList(timeStrings[keyIndex]);
        const std::vector<double> quaternionValues = ParseNumberList(valueStrings[keyIndex]);
        if (timeValues.empty() || quaternionValues.size() < 4)
        {
            continue;
        }

        MQuaternion correctedRotation(
            quaternionValues[0],
            quaternionValues[1],
            quaternionValues[2],
            quaternionValues[3]);
        if (applyTopLevelCorrection)
        {
            correctedRotation = correctionRotation * correctedRotation;
        }

        samples.times.push_back(timeValues[0]);
        samples.values.push_back(correctedRotation);
    }

    return MS::kSuccess;
}

MStatus AnimationImporter::buildSourceDeltaVector3Samples(
    const simple_dmx::Element *channelsClip,
    const simple_dmx::Element *targetElement,
    const MDagPath &targetPath,
    const SourceDeltaSettings &settings,
    Vector3AnimationSamples &samples) const
{
    if (!currentLogLayer_ || settings.mode == dcc_import_policy::SourceDeltaMode::None)
    {
        return extractVector3AnimationSamples(targetPath, currentLogLayer_, samples);
    }

    MStatus status = extractVector3AnimationSamples(targetPath, currentLogLayer_, samples);
    if (!status || samples.times.empty())
    {
        return status;
    }

    if (usesAnimationLayerForTransforms() &&
        (settings.mode == dcc_import_policy::SourceDeltaMode::Subtract ||
         settings.mode == dcc_import_policy::SourceDeltaMode::PreSubtract))
    {
        return MS::kSuccess;
    }

    if (settings.mode == dcc_import_policy::SourceDeltaMode::LinearDelta ||
        settings.mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
    {
        dcc_source_delta::ApplySourceDeltaLinearReferenceSamples(samples.values, settings.mode);
        return MS::kSuccess;
    }

    if (settings.useClip && !dcc_animation::IsEmptyAnimationLayerName(settings.sceneClipName.c_str()))
    {
        Vector3AnimationSamples referenceSamples;
        status = buildSceneLayerVector3Samples(MString(settings.sceneClipName.c_str()), targetPath, samples.times, referenceSamples);
        if (!status || referenceSamples.values.empty() || referenceSamples.values.size() != samples.values.size())
        {
            return maya_dmx::ReportError(MString("maya_dmx: sourceDelta scene layer was not usable: ") + settings.sceneClipName.c_str());
        }

        const size_t referenceIndex = std::min(static_cast<size_t>(settings.referenceFrame), referenceSamples.values.size() - 1);
        dcc_source_delta::ApplySourceDeltaReferenceValue(samples.values, referenceSamples.values[referenceIndex], settings.mode);

        return MS::kSuccess;
    }

    Vector3AnimationSamples referenceSamples;
    status = buildSceneReferenceVector3Samples(targetPath, samples.times, referenceSamples);
    if (!status || referenceSamples.values.empty() || referenceSamples.values.size() != samples.values.size())
    {
        return maya_dmx::ReportError("maya_dmx: sourceDelta reference scene samples were missing.");
    }

    dcc_source_delta::ApplySourceDeltaReferenceSamples(samples.values, referenceSamples.values, settings.mode);

    return MS::kSuccess;
}

MStatus AnimationImporter::buildSceneReferenceVector3Samples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    Vector3AnimationSamples &samples) const
{
    samples.times = times;
    samples.values.clear();
    if (times.empty())
    {
        return MS::kSuccess;
    }

    return dcc_animation::BuildSceneReferenceTranslationSamples(
        targetPath,
        times,
        MTime::kSeconds,
        samples.values);
}

MStatus AnimationImporter::buildSceneLayerVector3Samples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    Vector3AnimationSamples &samples) const
{
    samples.times = times;
    samples.values.clear();
    if (times.empty())
    {
        return MS::kSuccess;
    }

    return dcc_animation::BuildSceneLayerTranslationSamples(
        layerName,
        targetPath,
        times,
        MTime::kSeconds,
        samples.values);
}

MStatus AnimationImporter::buildSceneReferenceQuaternionSamples(
    const MDagPath &targetPath,
    const std::vector<double> &times,
    QuaternionAnimationSamples &samples) const
{
    samples.times = times;
    samples.values.clear();
    if (times.empty())
    {
        return MS::kSuccess;
    }

    return dcc_animation::BuildSceneReferenceQuaternionSamples(
        targetPath,
        times,
        MTime::kSeconds,
        samples.values);
}

MStatus AnimationImporter::buildSceneLayerQuaternionSamples(
    const MString &layerName,
    const MDagPath &targetPath,
    const std::vector<double> &times,
    QuaternionAnimationSamples &samples) const
{
    samples.times = times;
    samples.values.clear();
    if (times.empty())
    {
        return MS::kSuccess;
    }

    return dcc_animation::BuildSceneLayerQuaternionSamples(
        layerName,
        targetPath,
        times,
        MTime::kSeconds,
        samples.values);
}

MStatus AnimationImporter::buildSourceDeltaQuaternionSamples(
    const simple_dmx::Element *channelsClip,
    const simple_dmx::Element *targetElement,
    const MDagPath &targetPath,
    const SourceDeltaSettings &settings,
    QuaternionAnimationSamples &samples) const
{
    if (!currentLogLayer_ || settings.mode == dcc_import_policy::SourceDeltaMode::None)
    {
        return extractQuaternionAnimationSamples(targetPath, currentLogLayer_, samples);
    }

    MStatus status = extractQuaternionAnimationSamples(targetPath, currentLogLayer_, samples);
    if (!status || samples.times.empty())
    {
        return status;
    }

    if (usesAnimationLayerForTransforms() &&
        (settings.mode == dcc_import_policy::SourceDeltaMode::Subtract ||
         settings.mode == dcc_import_policy::SourceDeltaMode::PreSubtract))
    {
        return MS::kSuccess;
    }

    if (settings.mode == dcc_import_policy::SourceDeltaMode::LinearDelta ||
        settings.mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
    {
        dcc_source_delta::ApplySourceDeltaLinearReferenceSamples(samples.values, settings.mode);
        return MS::kSuccess;
    }

    if (settings.useClip && !dcc_animation::IsEmptyAnimationLayerName(settings.sceneClipName.c_str()))
    {
        QuaternionAnimationSamples referenceSamples;
        status = buildSceneLayerQuaternionSamples(MString(settings.sceneClipName.c_str()), targetPath, samples.times, referenceSamples);
        if (!status || referenceSamples.values.empty() || referenceSamples.values.size() != samples.values.size())
        {
            return maya_dmx::ReportError(MString("maya_dmx: sourceDelta scene layer was not usable: ") + settings.sceneClipName.c_str());
        }

        const size_t referenceIndex = std::min(static_cast<size_t>(settings.referenceFrame), referenceSamples.values.size() - 1);
        dcc_source_delta::ApplySourceDeltaReferenceValue(samples.values, referenceSamples.values[referenceIndex], settings.mode);

        return MS::kSuccess;
    }

    QuaternionAnimationSamples referenceSamples;
    status = buildSceneReferenceQuaternionSamples(targetPath, samples.times, referenceSamples);
    if (!status || referenceSamples.values.empty() || referenceSamples.values.size() != samples.values.size())
    {
        return maya_dmx::ReportError("maya_dmx: sourceDelta reference scene samples were missing.");
    }

    dcc_source_delta::ApplySourceDeltaReferenceSamples(samples.values, referenceSamples.values, settings.mode);

    return MS::kSuccess;
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

    return dcc_animation::SetCurveKeys(plug, times, values, curveType, MTime::kSeconds);
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

    return dcc_animation::SetCurveKeysAuto(plug, times, values, MTime::kSeconds);
}

MStatus AnimationImporter::setTransformCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType) const
{
    if (!usesAnimationLayerForTransforms())
    {
        return setCurveKeys(plug, times, values, curveType);
    }

    MString layerName;
    MStatus status = ensureAnimationLayer(layerName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendImportDebugLog((std::string("animLayer: set keys for ") + plug.name().asChar() + " on " + layerName.asChar()).c_str());
    return dcc_animation::SetCurveKeysOnAnimationLayer(
        plug,
        times,
        values,
        layerName,
        true,
        dcc_import_policy::UsesSourceDeltaImport(context_->scenePolicy));
}

MStatus AnimationImporter::applyVector3Animation(
    const simple_dmx::Element *channelsClip,
    const simple_dmx::Element *targetElement,
    const MDagPath &targetPath,
    const simple_dmx::Element *logLayer,
    const SourceDeltaSettings &settings) const
{
    if (!logLayer)
    {
        return MS::kSuccess;
    }

    Vector3AnimationSamples samples;
    MStatus status = buildSourceDeltaVector3Samples(channelsClip, targetElement, targetPath, settings, samples);
    if (!status || samples.times.empty())
    {
        return status;
    }

    std::vector<double> xValues;
    std::vector<double> yValues;
    std::vector<double> zValues;
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

    for (size_t valueIndex = 0; valueIndex < samples.values.size(); ++valueIndex)
    {
        const MVector finalValue = samples.values[valueIndex];
        xValues.push_back(finalValue.x);
        yValues.push_back(finalValue.y);
        zValues.push_back(finalValue.z);
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

    status = setTransformCurveKeys(translateXPlug, samples.times, xValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setTransformCurveKeys(translateYPlug, samples.times, yValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setTransformCurveKeys(translateZPlug, samples.times, zValues, MFnAnimCurve::kAnimCurveTL);
}

MStatus AnimationImporter::applyQuaternionAnimation(
    const simple_dmx::Element *channelsClip,
    const simple_dmx::Element *targetElement,
    const MDagPath &targetPath,
    const simple_dmx::Element *logLayer,
    const SourceDeltaSettings &settings) const
{
    if (!logLayer)
    {
        return MS::kSuccess;
    }

    QuaternionAnimationSamples samples;
    MStatus status = buildSourceDeltaQuaternionSamples(channelsClip, targetElement, targetPath, settings, samples);
    if (!status || samples.times.empty())
    {
        return status;
    }

    std::vector<double> xValues;
    std::vector<double> yValues;
    std::vector<double> zValues;
    MFnDependencyNode targetNodeFn(targetPath.node(), &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnTransform transformFn(targetPath, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MEulerRotation currentEulerRotation;
    status = transformFn.getRotation(currentEulerRotation);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (size_t valueIndex = 0; valueIndex < samples.values.size(); ++valueIndex)
    {
        const MQuaternion finalRotation = samples.values[valueIndex];
        MEulerRotation eulerRotation = finalRotation.asEulerRotation();
        eulerRotation.reorderIt(currentEulerRotation.order);
        xValues.push_back(eulerRotation.x);
        yValues.push_back(eulerRotation.y);
        zValues.push_back(eulerRotation.z);
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

    status = setTransformCurveKeys(rotateXPlug, samples.times, xValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setTransformCurveKeys(rotateYPlug, samples.times, yValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setTransformCurveKeys(rotateZPlug, samples.times, zValues, MFnAnimCurve::kAnimCurveTA);
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

    if (shouldSkipAppendScalarAnimation(targetPlug))
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

    if (!usesAnimationLayerForScalars())
    {
        return setCurveKeysAuto(targetPlug, times, values);
    }

    MString layerName;
    MStatus status = ensureAnimationLayer(layerName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return maya_cmd::SetKeyframesOnAnimationLayer(
        layerName,
        targetPlug,
        times.data(),
        values.data(),
        times.size(),
        true,
        dcc_import_policy::UsesSourceDeltaImport(context_->scenePolicy));
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

bool AnimationImporter::shouldSkipAppendTransformAnimation(const simple_dmx::Element *targetElement) const
{
    if (!targetElement || !dcc_import_policy::UsesAppendMissingObjects(context_->scenePolicy))
    {
        return false;
    }

    return context_->reusedTransformElementKeys.find(ElementKey(targetElement)) != context_->reusedTransformElementKeys.end();
}

bool AnimationImporter::shouldSkipAppendScalarAnimation(const MPlug &targetPlug) const
{
    if (!dcc_import_policy::UsesAppendMissingObjects(context_->scenePolicy) || targetPlug.isNull())
    {
        return false;
    }

    MPlugArray sourceConnections;
    MStatus status;
    const bool hasConnections = targetPlug.connectedTo(sourceConnections, true, false, &status);
    return status && hasConnections && sourceConnections.length() > 0;
}

bool AnimationImporter::isTopLevelImportedPath(const MDagPath &targetPath) const
{
    if (context_->sceneRoot.isNull())
    {
        return targetPath.length() == 1;
    }

    if (targetPath.length() < 2)
    {
        return false;
    }

    MDagPath parentPath(targetPath);
    if (parentPath.pop() != MS::kSuccess)
    {
        return false;
    }

    return parentPath.node() == context_->sceneRoot;
}

MObject AnimationImporter::findExistingControlNode(const std::string &controlNodeName, const MObject &sceneRoot) const
{
    if (!dcc_import_policy::UsesExistingObjectMerge(context_->scenePolicy) || controlNodeName.empty())
    {
        return MObject::kNullObj;
    }

    MStatus status;
    if (sceneRoot.isNull())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            if (dagIterator.depth() != 1)
            {
                continue;
            }

            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) != MS::kSuccess)
            {
                continue;
            }

            if (!(dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint)))
            {
                continue;
            }

            MFnDagNode dagNode(dagPath, &status);
            if (!status)
            {
                status = MS::kSuccess;
                continue;
            }

            if (dcc_import_policy::MatchesNodeNameForAppend(context_->scenePolicy, dagNode.name().asChar(), controlNodeName))
            {
                return dagPath.node();
            }
        }

        return MObject::kNullObj;
    }

    MFnDagNode sceneRootFn(sceneRoot, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < sceneRootFn.childCount(); ++childIndex)
    {
        const MObject childObject = sceneRootFn.child(childIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (!(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MFnDagNode childFn(childObject, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (dcc_import_policy::MatchesNodeNameForAppend(context_->scenePolicy, childFn.name().asChar(), controlNodeName))
        {
            return childObject;
        }
    }

    return MObject::kNullObj;
}

MStatus AnimationImporter::registerImportedControlPath(const MObject &controlNodeObject)
{
    if (controlNodeObject.isNull())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MDagPath controlPath;
    status = MDagPath::getAPathTo(controlNodeObject, controlPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const std::string fullPathName = controlPath.fullPathName().asChar();
    for (const MDagPath &existingPath : context_->importedControlPaths)
    {
        if (fullPathName == existingPath.fullPathName().asChar())
        {
            return MS::kSuccess;
        }
    }

    context_->importedControlPaths.push_back(controlPath);
    return MS::kSuccess;
}

void AnimationImporter::registerScalarTargetBinding(
    const std::string &targetName,
    const dmx_import_translator::ScalarAttributeBinding &binding)
{
    if (targetName.empty() || binding.node.isNull() || binding.attributeName.empty())
    {
        return;
    }

    std::vector<ScalarAttributeBinding> &bindings = context_->importedScalarTargets[targetName];
    for (const ScalarAttributeBinding &existingBinding : bindings)
    {
        if (existingBinding.node == binding.node && existingBinding.attributeName == binding.attributeName)
        {
            return;
        }
    }

    bindings.push_back(binding);
}

const simple_dmx::Element *AnimationImporter::FindAnimationList() const
{
    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, documentRoot_, "animationList"))
    {
        const_cast<AnimationImporter *>(this)->animationList_ = animationList;
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, importRoot_, "animationList"))
    {
        const_cast<AnimationImporter *>(this)->animationList_ = animationList;
        return animationList;
    }

    if (const simple_dmx::Element *animationList = FindAttributeElement(context_->document, modelRoot_, "animationList"))
    {
        const_cast<AnimationImporter *>(this)->animationList_ = animationList;
        return animationList;
    }

    const_cast<AnimationImporter *>(this)->animationList_ = nullptr;
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

bool AnimationImporter::usesAnimationLayerForTransforms() const
{
    return dcc_import_policy::UsesAnimationLayerImport(context_->scenePolicy);
}

bool AnimationImporter::usesAnimationLayerForScalars() const
{
    return dcc_import_policy::UsesAnimationLayerImport(context_->scenePolicy);
}

MStatus AnimationImporter::ensureAnimationLayer(MString &layerName) const
{
    const std::string configuredName = context_->scenePolicy.animationLayerName.empty() ?
        std::string("dmx_anim") :
        context_->scenePolicy.animationLayerName;
    MStatus status = dcc_animation::EnsureAnimationLayerCached(
        configuredName.c_str(),
        context_->scenePolicy.animationImportMode == dcc_import_policy::AnimationImportMode::ReplaceLayer,
        dcc_import_policy::UsesSourceDeltaImport(context_->scenePolicy),
        true,
        transformAnimationLayerCache_,
        &layerName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendImportDebugLog((std::string("animLayer: ensured layer ") + layerName.asChar()).c_str());
    return MS::kSuccess;
}

MStatus AnimationImporter::ApplyChannelsClipAnimation(const simple_dmx::Element *channelsClip)
{
    if (!channelsClip)
    {
        return MS::kSuccess;
    }

    const SourceDeltaSettings sourceDeltaSettings = resolveSourceDeltaSettings(channelsClip);
    if (!shouldImportChannelsClip(channelsClip, sourceDeltaSettings))
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
            if (shouldSkipAppendTransformAnimation(currentTargetElement_))
            {
                continue;
            }
            status = applyVector3Animation(
                channelsClip,
                currentTargetElement_,
                targetIt->second,
                currentLogLayer_,
                sourceDeltaSettings);
        }
        else if (targetIt != context_->importedTransformPaths.end() && currentTargetAttribute_ == "orientation")
        {
            if (shouldSkipAppendTransformAnimation(currentTargetElement_))
            {
                continue;
            }
            status = applyQuaternionAnimation(
                channelsClip,
                currentTargetElement_,
                targetIt->second,
                currentLogLayer_,
                sourceDeltaSettings);
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

    std::string controlNodeName = combinationOperator->name.empty()
        ? "combinationControls"
        : SanitizeNodeName(combinationOperator->name + std::string("_controls"));

    MStatus status;
    MObject controlNodeObject = findExistingControlNode(controlNodeName, sceneRoot);
    const bool reusedExistingNode = !controlNodeObject.isNull();
    if (!reusedExistingNode)
    {
        if (dcc_import_policy::UsesAnimationOnlyImport(context_->scenePolicy))
        {
            return MS::kSuccess;
        }

        MFnTransform controlNodeFn;
        controlNodeObject = controlNodeFn.create(sceneRoot, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        controlNodeFn.setName(controlNodeName.c_str());
    }

    status = registerImportedControlPath(controlNodeObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

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

        MObject attributeObject = controlDependencyNode.attribute(controlName.c_str(), &status);
        const bool reusedExistingAttribute = status && !attributeObject.isNull();
        if (!reusedExistingAttribute)
        {
            status = MS::kSuccess;
            MFnNumericAttribute numericAttributeFn;
            attributeObject = numericAttributeFn.create(
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
        if (!(reusedExistingNode && reusedExistingAttribute))
        {
            status = controlPlug.setFloat(defaultValue);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }

        ScalarAttributeBinding binding{controlNodeObject, controlName};
        registerScalarTargetBinding(control->name, binding);
        for (const std::string &rawControlName : FindAttributeStringArray(control, "rawControlNames"))
        {
            registerScalarTargetBinding(rawControlName, binding);
        }
    }

    return MS::kSuccess;
}


} // namespace dmx_import_impl
