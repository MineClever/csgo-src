#include "SmdSceneImporter.h"
#include "SmdAnimationImporter.h"
#include "SmdImportUtils.h"
#include "SmdMeshImporter.h"

#include <common/ImportTransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>

#include <maya/MFnDagNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>

SmdSceneImporter::SmdSceneImporter(std::shared_ptr<const simple_smd::Document> document, const SmdImportOptions &importOptions)
    : document_(document)
    , importOptions_(importOptions)
    , mergeResolver_(importOptions.scenePolicy)
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

    SmdAnimationImporter animationImporter(
        document_,
        importOptions_,
        mergeResolver_,
        jointPathsByBone_,
        reusedBoneIndices_);
    status = animationImporter.Apply();
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (!mergeResolver_.usesAnimationOnlyImport())
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
    if (mergeResolver_.usesSceneRoot() ||
        mergeResolver_.usesAnimationOnlyImport())
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
    const bool reuseExistingMode = mergeResolver_.usesExistingObjectMerge();
    if (reuseExistingMode)
    {
        jointObject = findExistingJoint(node);
    }

    const bool reusedExistingJoint = !jointObject.isNull();
    if (mergeResolver_.usesAnimationOnlyImport() && !reusedExistingJoint)
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

        jointFn.setName(smd_import_impl::SanitizeNodeName(node.name).c_str());
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
    return mergeResolver_.findAppendTargetChild(findParentObject(node), smd_import_impl::SanitizeNodeName(node.name), true);
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
            (mergeResolver_.usesAppendMissingObjects() ||
             mergeResolver_.usesAnimationLayerImport() ||
             mergeResolver_.usesAnimationOnlyImport()))
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
        const MStatus status = smd_import_impl::SetPoseOnObject(pathIt->second.node(), *pose, correction);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

