#pragma once

#include "../common_smd/SimpleSmdDocument.h"

#include <memory>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <unordered_map>

class SmdMeshImporter
{
public:
    SmdMeshImporter(
        std::shared_ptr<const simple_smd::Document> document,
        std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone);

    MStatus Import(MObject parent) const;

private:
    MStatus importMaterialGroup(const std::string &materialName, MObject parent) const;
    MStatus assignMaterial(const std::string &materialName, const MObject &meshObject) const;
    MObject findPrimaryMeshChild(const MObject &transformObject) const;
    MStatus createSkinClusterWithApi(
        const MDagPathArray &influencePaths,
        const MDagPath &meshDagPath,
        const MDagPath &meshParentPath,
        MObject &skinClusterObject) const;
    MStatus applySkinning(
        const std::vector<std::vector<simple_smd::TriangleWeight>> &vertexLinks,
        const MDagPath &meshTransformPath) const;

    std::shared_ptr<const simple_smd::Document> document_;
    std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone_;
};
