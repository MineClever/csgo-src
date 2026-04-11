#pragma once

#include "../common_smd/SimpleSmdDocument.h"

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <unordered_map>

class SmdSceneImporter
{
public:
    explicit SmdSceneImporter(const simple_smd::Document &document);

    MStatus Import();

private:
    MStatus createImportRoot();
    MStatus createJointHierarchy();
    MStatus applyBindPose();
    MStatus applyAnimation();
    MStatus createJoint(const simple_smd::Node &node);

    MObject findParentObject(const simple_smd::Node &node) const;
    const simple_smd::SkeletonPose *findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const;

    const simple_smd::Document &document_;
    MObject importRoot_ = MObject::kNullObj;
    std::unordered_map<int, MDagPath> jointPathsByBone_;
};
