#pragma once

#include "SmdImportSession.h"
#include "SmdSourceDeltaProcessor.h"

#include <common/AnimationCurveUtils.h>
#include <common/SceneMergeStrategy.h>
#include <common_smd/SimpleSmdDocument.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>

class SmdAnimationImporter
{
public:
    SmdAnimationImporter(
        std::shared_ptr<const simple_smd::Document> document,
        const SmdImportOptions &importOptions,
        const dcc_import_policy::SceneMergeResolver &mergeResolver,
        const std::unordered_map<int, MDagPath> &jointPathsByBone,
        const std::unordered_set<int> &reusedBoneIndices);

    MStatus Apply();

private:
    const simple_smd::SkeletonPose *findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const;
    bool isTopLevelNode(const simple_smd::Node &node) const;
    bool usesAnimationLayerForTransforms() const;
    MStatus ensureTransformAnimationLayer(MString &layerName) const;
    MStatus setCurveKeys(
        const MPlug &plug,
        const std::vector<double> &times,
        const std::vector<double> &values,
        MFnAnimCurve::AnimCurveType curveType) const;
    MStatus setLayerCurveKeys(
        const MString &layerName,
        const MPlug &plug,
        const std::vector<double> &times,
        const std::vector<double> &values,
        bool keepAdditiveMode) const;

    std::shared_ptr<const simple_smd::Document> document_;
    const SmdImportOptions &importOptions_;
    const dcc_import_policy::SceneMergeResolver &mergeResolver_;
    const std::unordered_map<int, MDagPath> &jointPathsByBone_;
    const std::unordered_set<int> &reusedBoneIndices_;
    SmdSourceDeltaProcessor sourceDeltaProcessor_;
    mutable dcc_animation::AnimationLayerCache transformAnimationLayerCache_;
};
