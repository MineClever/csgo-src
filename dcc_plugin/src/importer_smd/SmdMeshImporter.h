#pragma once

#include <common_smd/SimpleSmdDocument.h>
#include <common/ImportPolicy.h>
#include <common/TransformCorrection.h>

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
        std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone,
        dcc_import_policy::SceneImportPolicy scenePolicy,
        dcc_import_transform::TransformCorrection transformCorrection,
        bool flipUvV);

    MStatus Import(MObject parent) const;

private:
    MStatus importMaterialGroup(const std::string &materialName, MObject parent) const;
    MStatus assignMaterial(const std::string &materialName, const MObject &meshObject) const;
    MObject findPrimaryMeshChild(const MObject &transformObject) const;
    bool meshTopologyMatches(
        const MObject &meshObject,
        const MIntArray &polygonCounts,
        const MIntArray &polygonConnects) const;
    MObject findExistingSkinClusterNode(const MObject &meshObject) const;
    MStatus updateExistingSkinClusterBindings(
        const MObject &skinClusterObject,
        const MDagPathArray &influencePaths,
        const MDagPath &meshParentPath) const;
    MStatus createSkinClusterWithApi(
        const MDagPathArray &influencePaths,
        const MDagPath &meshDagPath,
        const MDagPath &meshParentPath,
        MObject &skinClusterObject) const;
    MStatus applySkinning(
        const std::vector<std::vector<simple_smd::TriangleWeight>> &vertexLinks,
        const MDagPath &meshTransformPath) const;
    MObject findExistingMeshGroup(MObject parent, const std::string &materialName) const;

    std::shared_ptr<const simple_smd::Document> document_;
    std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone_;
    dcc_import_policy::SceneImportPolicy scenePolicy_;
    dcc_import_transform::TransformCorrection transformCorrection_;
    bool flipUvV_ = true;
};
