#include "SmdSceneImporter.h"
#include "SmdMeshImporter.h"

#include <common/MayaCommandUtils.h>
#include <common/ImportTransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

namespace
{
double ApplySplineWeight(double t)
{
    return 3.0 * t * t - 2.0 * t * t * t;
}

std::string SanitizeNodeName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("smd_node") : value;
}

MStatus SetPoseOnObject(
    MObject object,
    const simple_smd::SkeletonPose &pose,
    const dcc_import_transform::TransformCorrection *correction)
{
    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access transform for skeleton pose.", status);
    }

    status = transformFn.setTranslation(MVector(pose.tx, pose.ty, pose.tz), MSpace::kTransform);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton translation.", status);
    }

    status = transformFn.setRotation(MEulerRotation(pose.rx, pose.ry, pose.rz));
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton rotation.", status);
    }

    if (correction && !correction->IsIdentity())
    {
        status = dcc_import_transform::ApplyPreTransformToObject(object, correction->Matrix());
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to apply top-level import correction to skeleton pose.", status);
        }
    }

    return MS::kSuccess;
}

MStatus SetCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType)
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
        return maya_smd::ReportError(MString("maya_smd: failed to create animation curve for ") + plug.name(), status);
    }

    for (size_t index = 0; index < times.size(); ++index)
    {
        curveFn.addKey(
            MTime(times[index], MTime::uiUnit()),
            values[index],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to add animation key for ") + plug.name(), status);
        }
    }

    return MS::kSuccess;
}

MStatus SetLayerCurveKeys(
    const MString &layerName,
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    return maya_cmd::SetKeyframesOnAnimationLayer(layerName, plug, times.data(), values.data(), times.size(), false);
}

}

SmdSceneImporter::SmdSceneImporter(std::shared_ptr<const simple_smd::Document> document, const SmdImportOptions &importOptions)
    : document_(document)
    , importOptions_(importOptions)
{
}

MStatus SmdSceneImporter::Import()
{
    MStatus status = createImportRoot();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = createJointHierarchy();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = applyBindPose();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = applyAnimation();
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (!dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy))
    {
        auto jointPathsByBonePtr = std::shared_ptr<const std::unordered_map<int, MDagPath>>(
            &jointPathsByBone_,
            [](const std::unordered_map<int, MDagPath> *) {});
        SmdMeshImporter meshImporter(document_, jointPathsByBonePtr, importOptions_.scenePolicy, importOptions_.transformCorrection);
        status = meshImporter.Import(importRoot_);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    if (document_->hasVertexAnimation)
    {
        maya_smd::ReportWarning("maya_smd: vertexanimation section detected but not implemented yet");
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createImportRoot()
{
    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy) ||
        dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy))
    {
        importRoot_ = MObject::kNullObj;
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform rootTransformFn;
    importRoot_ = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to create SMD import root.", status);
    }

    rootTransformFn.setName("smd_import_root#");

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createJointHierarchy()
{
    for (const simple_smd::Node &node : document_->nodes)
    {
        const MStatus status = createJoint(node);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createJoint(const simple_smd::Node &node)
{
    MStatus status;
    if (skippedBoneIndices_.find(node.parentIndex) != skippedBoneIndices_.end())
    {
        skippedBoneIndices_.insert(node.index);
        return MS::kSuccess;
    }

    const MObject parentObject = findParentObject(node);
    MObject jointObject = MObject::kNullObj;
    const bool reuseExistingMode = dcc_import_policy::UsesExistingObjectMerge(importOptions_.scenePolicy);
    if (reuseExistingMode)
    {
        jointObject = findExistingJoint(node);
    }

    const bool reusedExistingJoint = !jointObject.isNull();
    if (dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy) && !reusedExistingJoint)
    {
        skippedBoneIndices_.insert(node.index);
        return MS::kSuccess;
    }

    if (!reusedExistingJoint)
    {
        MFnIkJoint jointFn;
        jointObject = jointFn.create(parentObject, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create joint for node ") + node.name.c_str(), status);
        }

        jointFn.setName(SanitizeNodeName(node.name).c_str());
    }
    else
    {
        reusedBoneIndices_.insert(node.index);
    }

    MDagPath jointPath;
    status = MDagPath::getAPathTo(jointObject, jointPath);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve DAG path for joint ") + node.name.c_str(), status);
    }

    jointPathsByBone_[node.index] = jointPath;
    return MS::kSuccess;
}

MObject SmdSceneImporter::findExistingJoint(const simple_smd::Node &node) const
{
    return findAppendTargetChild(findParentObject(node), SanitizeNodeName(node.name));
}

MObject SmdSceneImporter::findAppendTargetChild(const MObject &parent, const std::string &nodeName) const
{
    MStatus status;
    if (parent.isNull())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) != MS::kSuccess || !dagPath.hasFn(MFn::kJoint))
            {
                continue;
            }

            MFnDagNode dagNode(dagPath, &status);
            if (status && dcc_import_policy::MatchesNodeNameForAppend(importOptions_.scenePolicy, dagNode.name().asChar(), nodeName))
            {
                return dagPath.node();
            }
        }

        return MObject::kNullObj;
    }

    MFnDagNode parentDagNode(parent, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < parentDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = parentDagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kJoint))
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDagNode childDagNode(childObject, &status);
        if (status && dcc_import_policy::MatchesNodeNameForAppend(importOptions_.scenePolicy, childDagNode.name().asChar(), nodeName))
        {
            return childObject;
        }
        status = MS::kSuccess;
    }

    return MObject::kNullObj;
}

MObject SmdSceneImporter::findParentObject(const simple_smd::Node &node) const
{
    auto parentIt = jointPathsByBone_.find(node.parentIndex);
    if (parentIt == jointPathsByBone_.end())
    {
        return importRoot_;
    }

    return parentIt->second.node();
}

const simple_smd::SkeletonPose *SmdSceneImporter::findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const
{
    const auto poseIt = std::find_if(
        frame.poses.begin(),
        frame.poses.end(),
        [boneIndex](const simple_smd::SkeletonPose &pose)
        {
            return pose.boneIndex == boneIndex;
        });
    return poseIt == frame.poses.end() ? nullptr : &(*poseIt);
}

bool SmdSceneImporter::isTopLevelNode(const simple_smd::Node &node) const
{
    return jointPathsByBone_.find(node.parentIndex) == jointPathsByBone_.end();
}

MStatus SmdSceneImporter::applyBindPose()
{
    if (document_->skeletonFrames.empty())
    {
        return MS::kSuccess;
    }

    const simple_smd::SkeletonFrame &bindFrame = document_->skeletonFrames.front();
    for (const simple_smd::Node &node : document_->nodes)
    {
        if (reusedBoneIndices_.find(node.index) != reusedBoneIndices_.end() &&
            (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy) ||
             dcc_import_policy::UsesAnimationLayerImport(importOptions_.scenePolicy) ||
             dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy)))
        {
            continue;
        }

        const simple_smd::SkeletonPose *pose = findPose(bindFrame, node.index);
        if (!pose)
        {
            continue;
        }

        const auto pathIt = jointPathsByBone_.find(node.index);
        if (pathIt == jointPathsByBone_.end())
        {
            continue;
        }

        const dcc_import_transform::TransformCorrection *correction =
            isTopLevelNode(node) ? &importOptions_.transformCorrection : nullptr;
        const MStatus status = SetPoseOnObject(pathIt->second.node(), *pose, correction);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::applyAnimation()
{
    if (document_->skeletonFrames.size() <= 1)
    {
        return MS::kSuccess;
    }

    for (const simple_smd::Node &node : document_->nodes)
    {
        if (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy) &&
            reusedBoneIndices_.find(node.index) != reusedBoneIndices_.end())
        {
            continue;
        }

        const auto pathIt = jointPathsByBone_.find(node.index);
        if (pathIt == jointPathsByBone_.end())
        {
            continue;
        }

        MStatus status;
        MFnDependencyNode dependencyNodeFn(pathIt->second.node(), &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access dependency node for animated joint ") + node.name.c_str(), status);
        }

        std::vector<double> times;
        std::vector<double> txValues;
        std::vector<double> tyValues;
        std::vector<double> tzValues;
        std::vector<double> rxValues;
        std::vector<double> ryValues;
        std::vector<double> rzValues;
        std::vector<MVector> sampledTranslations;
        std::vector<MQuaternion> sampledRotations;
        const bool applyTopLevelCorrection =
            isTopLevelNode(node) && !importOptions_.transformCorrection.IsIdentity();
        const MQuaternion correctionRotation = importOptions_.transformCorrection.RotationQuaternion();

        for (const simple_smd::SkeletonFrame &frame : document_->skeletonFrames)
        {
            const simple_smd::SkeletonPose *pose = findPose(frame, node.index);
            if (!pose)
            {
                continue;
            }

            MVector correctedTranslation(pose->tx, pose->ty, pose->tz);
            if (applyTopLevelCorrection)
            {
                correctedTranslation = dcc_import_transform::ApplyToPoint(importOptions_.transformCorrection, correctedTranslation);
            }

            MQuaternion correctedRotation = MEulerRotation(pose->rx, pose->ry, pose->rz).asQuaternion();
            if (applyTopLevelCorrection)
            {
                correctedRotation = correctionRotation * correctedRotation;
            }
            times.push_back(static_cast<double>(frame.time));
            sampledTranslations.push_back(correctedTranslation);
            sampledRotations.push_back(correctedRotation);
        }

        if (times.size() <= 1)
        {
            continue;
        }

        status = applySourceDeltaToSamples(sampledTranslations, sampledRotations);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MFnTransform transformFn(pathIt->second, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access transform for animated joint ") + node.name.c_str(), status);
        }

        MEulerRotation currentEulerRotation;
        status = transformFn.getRotation(currentEulerRotation);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to read current rotation for animated joint ") + node.name.c_str(), status);
        }

        for (size_t valueIndex = 0; valueIndex < sampledTranslations.size(); ++valueIndex)
        {
            const MVector finalTranslation = sampledTranslations[valueIndex];
            txValues.push_back(finalTranslation.x);
            tyValues.push_back(finalTranslation.y);
            tzValues.push_back(finalTranslation.z);

            const MQuaternion finalRotation = sampledRotations[valueIndex];
            MEulerRotation finalEuler = finalRotation.asEulerRotation();
            finalEuler.reorderIt(currentEulerRotation.order);
            rxValues.push_back(finalEuler.x);
            ryValues.push_back(finalEuler.y);
            rzValues.push_back(finalEuler.z);
        }

        const MPlug translateXPlug = dependencyNodeFn.findPlug("translateX", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateX plug for animated joint.", status);
        }
        MString layerName;
        const bool useLayer = usesAnimationLayerForTransforms();
        if (useLayer)
        {
            status = ensureTransformAnimationLayer(layerName);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }

        status = useLayer ? SetLayerCurveKeys(layerName, translateXPlug, times, txValues) : SetCurveKeys(translateXPlug, times, txValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateY plug for animated joint.", status);
        }
        const MPlug translateYPlug = dependencyNodeFn.findPlug("translateY", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateZ plug for animated joint.", status);
        }
        status = useLayer ? SetLayerCurveKeys(layerName, translateYPlug, times, tyValues) : SetCurveKeys(translateYPlug, times, tyValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateX plug for animated joint.", status);
        }
        const MPlug translateZPlug = dependencyNodeFn.findPlug("translateZ", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateY plug for animated joint.", status);
        }
        status = useLayer ? SetLayerCurveKeys(layerName, translateZPlug, times, tzValues) : SetCurveKeys(translateZPlug, times, tzValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateZ plug for animated joint.", status);
        }
        const MPlug rotateXPlug = dependencyNodeFn.findPlug("rotateX", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = useLayer ? SetLayerCurveKeys(layerName, rotateXPlug, times, rxValues) : SetCurveKeys(rotateXPlug, times, rxValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
        const MPlug rotateYPlug = dependencyNodeFn.findPlug("rotateY", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = useLayer ? SetLayerCurveKeys(layerName, rotateYPlug, times, ryValues) : SetCurveKeys(rotateYPlug, times, ryValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
        const MPlug rotateZPlug = dependencyNodeFn.findPlug("rotateZ", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = useLayer ? SetLayerCurveKeys(layerName, rotateZPlug, times, rzValues) : SetCurveKeys(rotateZPlug, times, rzValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    MAnimControl::setMinTime(MTime(static_cast<double>(document_->skeletonFrames.front().time), MTime::uiUnit()));
    MAnimControl::setMaxTime(MTime(static_cast<double>(document_->skeletonFrames.back().time), MTime::uiUnit()));
    MAnimControl::setCurrentTime(MTime(static_cast<double>(document_->skeletonFrames.front().time), MTime::uiUnit()));
    return MS::kSuccess;
}

MStatus SmdSceneImporter::applySourceDeltaToSamples(
    std::vector<MVector> &translations,
    std::vector<MQuaternion> &rotations) const
{
    const dcc_import_policy::SourceDeltaMode mode = importOptions_.scenePolicy.sourceDeltaMode;
    if (mode == dcc_import_policy::SourceDeltaMode::None)
    {
        return MS::kSuccess;
    }

    if (translations.empty() || rotations.empty() || translations.size() != rotations.size())
    {
        return MS::kSuccess;
    }

    if (mode != dcc_import_policy::SourceDeltaMode::LinearDelta &&
        mode != dcc_import_policy::SourceDeltaMode::SplineDelta)
    {
        return maya_smd::ReportError("maya_smd: sourceDelta currently only supports lineardelta and splinedelta for SMD transform import.");
    }

    const size_t sampleCount = translations.size();
    const MVector firstTranslation = translations.front();
    const MVector lastTranslation = translations.back();
    const MQuaternion firstRotation = rotations.front();
    const MQuaternion lastRotation = rotations.back();
    for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        double t = sampleCount > 1 ? static_cast<double>(sampleIndex) / static_cast<double>(sampleCount - 1) : 1.0;
        if (mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
        {
            t = ApplySplineWeight(t);
        }

        const MVector referenceTranslation = firstTranslation * (1.0 - t) + lastTranslation * t;
        translations[sampleIndex] = translations[sampleIndex] - referenceTranslation;

        const MQuaternion referenceRotation = slerp(firstRotation, lastRotation, t);
        rotations[sampleIndex] = rotations[sampleIndex] * referenceRotation.inverse();
    }

    return MS::kSuccess;
}

bool SmdSceneImporter::usesAnimationLayerForTransforms() const
{
    return dcc_import_policy::UsesAnimationLayerImport(importOptions_.scenePolicy);
}

MStatus SmdSceneImporter::ensureTransformAnimationLayer(MString &layerName) const
{
    if (transformAnimationLayerInitialized_)
    {
        layerName = transformAnimationLayerName_;
        return layerName.length() > 0 ? MS::kSuccess : MS::kFailure;
    }

    transformAnimationLayerInitialized_ = true;
    const std::string configuredName = importOptions_.scenePolicy.animationLayerName.empty() ?
        std::string("smd_layer") :
        importOptions_.scenePolicy.animationLayerName;
    MStatus status = maya_cmd::EnsureAnimationLayer(
        configuredName.c_str(),
        importOptions_.scenePolicy.animationImportMode == dcc_import_policy::AnimationImportMode::ReplaceLayer,
        true,
        &transformAnimationLayerName_);
    if (!status)
    {
        transformAnimationLayerName_.clear();
        return MStatus::kFailure;
    }

    layerName = transformAnimationLayerName_;
    return MS::kSuccess;
}
