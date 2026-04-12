#pragma once

#include "SmdImportSession.h"

#include <common_smd/SimpleSmdDocument.h>

#include <memory>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <unordered_map>
#include <unordered_set>

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
    MObject findAppendTargetChild(const MObject &parent, const std::string &nodeName) const;

    MObject findParentObject(const simple_smd::Node &node) const;
    const simple_smd::SkeletonPose *findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const;
    bool isTopLevelNode(const simple_smd::Node &node) const;

    std::shared_ptr<const simple_smd::Document> document_;
    SmdImportOptions importOptions_;
    MObject importRoot_ = MObject::kNullObj;
    std::unordered_map<int, MDagPath> jointPathsByBone_;
    std::unordered_set<int> reusedBoneIndices_;
};
