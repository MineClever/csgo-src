#include "DmxImportAnimation.h"
#include "DmxImportInternals.h"

#include <common/MayaCommandUtils.h>
#include <common/ImportTransformCorrection.h>

#include <algorithm>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MDGModifier.h>
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

static MStatus ClearExistingAnimationCurve(const MPlug &plug)
{
    if (plug.isNull())
    {
        return MS::kSuccess;
    }

    MPlugArray sourceConnections;
    MStatus status;
    plug.connectedTo(sourceConnections, true, false, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (unsigned int sourceIndex = 0; sourceIndex < sourceConnections.length(); ++sourceIndex)
    {
        const MPlug sourcePlug = sourceConnections[sourceIndex];
        if (sourcePlug.isNull() || !sourcePlug.node().hasFn(MFn::kAnimCurve))
        {
            continue;
        }

        MDGModifier disconnectModifier;
        status = disconnectModifier.disconnect(sourcePlug, plug);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = disconnectModifier.doIt();
        if (!status)
        {
            return MStatus::kFailure;
        }

        MDGModifier deleteModifier;
        status = deleteModifier.deleteNode(sourcePlug.node());
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = deleteModifier.doIt();
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
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

    MStatus status = ClearExistingAnimationCurve(plug);
    if (!status)
    {
        return MStatus::kFailure;
    }

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

    MStatus status = ClearExistingAnimationCurve(plug);
    if (!status)
    {
        return MStatus::kFailure;
    }

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
    return maya_cmd::SetKeyframesOnAnimationLayer(layerName, plug, times.data(), values.data(), times.size(), true);
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
    std::vector<MVector> sampledTranslations;
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

        times.push_back(timeValues[0]);
        sampledTranslations.push_back(correctedValue);
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

    for (size_t valueIndex = 0; valueIndex < sampledTranslations.size(); ++valueIndex)
    {
        const MVector finalValue = sampledTranslations[valueIndex];
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

    status = setTransformCurveKeys(translateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setTransformCurveKeys(translateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTL);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setTransformCurveKeys(translateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTL);
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
    std::vector<MQuaternion> sampledRotations;
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

        times.push_back(timeValues[0]);
        sampledRotations.push_back(correctedRotation);
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

    for (size_t valueIndex = 0; valueIndex < sampledRotations.size(); ++valueIndex)
    {
        const MQuaternion finalRotation = sampledRotations[valueIndex];
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

    status = setTransformCurveKeys(rotateXPlug, times, xValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = setTransformCurveKeys(rotateYPlug, times, yValues, MFnAnimCurve::kAnimCurveTA);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return setTransformCurveKeys(rotateZPlug, times, zValues, MFnAnimCurve::kAnimCurveTA);
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

    return maya_cmd::SetKeyframesOnAnimationLayer(layerName, targetPlug, times.data(), values.data(), times.size(), true);
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
    if (transformAnimationLayerInitialized_)
    {
        layerName = transformAnimationLayerName_;
        return layerName.length() > 0 ? MS::kSuccess : MS::kFailure;
    }

    transformAnimationLayerInitialized_ = true;
    const std::string configuredName = context_->scenePolicy.animationLayerName.empty() ?
        std::string("dmx_anim") :
        context_->scenePolicy.animationLayerName;
    MStatus status = maya_cmd::EnsureAnimationLayer(
        configuredName.c_str(),
        context_->scenePolicy.animationImportMode == dcc_import_policy::AnimationImportMode::ReplaceLayer,
        true,
        &transformAnimationLayerName_);
    if (!status)
    {
        transformAnimationLayerName_.clear();
        return MStatus::kFailure;
    }

    AppendImportDebugLog((std::string("animLayer: ensured layer ") + transformAnimationLayerName_.asChar()).c_str());
    layerName = transformAnimationLayerName_;
    return MS::kSuccess;
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
            if (shouldSkipAppendTransformAnimation(currentTargetElement_))
            {
                continue;
            }
            status = applyVector3Animation(targetIt->second, currentLogLayer_);
        }
        else if (targetIt != context_->importedTransformPaths.end() && currentTargetAttribute_ == "orientation")
        {
            if (shouldSkipAppendTransformAnimation(currentTargetElement_))
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
