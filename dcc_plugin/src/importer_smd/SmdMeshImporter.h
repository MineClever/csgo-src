#pragma once

#include "../common_smd/SimpleSmdDocument.h"

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <unordered_map>

class SmdMeshImporter
{
public:
    SmdMeshImporter(
        const simple_smd::Document &document,
        const std::unordered_map<int, MDagPath> &jointPathsByBone);

    MStatus Import(MObject parent) const;

private:
    MStatus importMaterialGroup(const std::string &materialName, MObject parent) const;
    MObject findPrimaryMeshChild(const MObject &transformObject) const;
    MStatus createSkinClusterWithApi(
        const MDagPathArray &influencePaths,
        const MDagPath &meshDagPath,
        const MDagPath &meshParentPath,
        MObject &skinClusterObject) const;
    MStatus applySkinning(
        const std::vector<std::vector<simple_smd::TriangleWeight>> &vertexLinks,
        const MDagPath &meshTransformPath) const;

    const simple_smd::Document &document_;
    const std::unordered_map<int, MDagPath> &jointPathsByBone_;
};
