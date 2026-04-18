#pragma once

#include "SmdImportSession.h"

#include <common/SceneMergeStrategy.h>
#include <common_smd/SimpleSmdDocument.h>

#include <memory>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

class SmdSceneImporter
{
public:
    SmdSceneImporter(std::shared_ptr<const simple_smd::Document> document, const SmdImportOptions &importOptions);

    MStatus Import();

private:
    MStatus createImportRoot();
    MStatus createJointHierarchy();
    MStatus applyBindPose();
    MStatus applyAnimation();
    MStatus createJoint(const simple_smd::Node &node);
    MObject findExistingJoint(const simple_smd::Node &node) const;

    MObject findParentObject(const simple_smd::Node &node) const;
    const simple_smd::SkeletonPose *findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const;
    bool isTopLevelNode(const simple_smd::Node &node) const;
    MStatus applySourceDeltaToSamples(
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;
    MStatus buildSceneReferenceSamples(
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;
    MStatus buildSceneLayerSamples(
        const MString &layerName,
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;
    MStatus ensureTransformAnimationLayer(MString &layerName) const;
    bool usesAnimationLayerForTransforms() const;

    std::shared_ptr<const simple_smd::Document> document_;
    SmdImportOptions importOptions_;
    dcc_import_policy::SceneMergeResolver mergeResolver_;
    MObject importRoot_ = MObject::kNullObj;
    std::unordered_map<int, MDagPath> jointPathsByBone_;
    std::unordered_set<int> reusedBoneIndices_;
    std::unordered_set<int> skippedBoneIndices_;
    mutable bool transformAnimationLayerInitialized_ = false;
    mutable MString transformAnimationLayerName_;
};
