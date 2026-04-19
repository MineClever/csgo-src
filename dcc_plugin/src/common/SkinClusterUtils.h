#pragma once

#include <common/ImportPolicy.h>

#include <unordered_map>
#include <string>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dcc_skinning
{

std::unordered_map<std::string, unsigned int> BuildInfluenceIndexByPath(const MDagPathArray &influencePaths);

bool FindMatchingInfluencePath(
    const dcc_import_policy::SceneImportPolicy &scenePolicy,
    const MDagPathArray &influencePaths,
    const MDagPath &requiredInfluencePath,
    MDagPath &matchedInfluencePath);

MStatus EnsureSkinClusterContainsInfluences(
    const dcc_import_policy::SceneImportPolicy &scenePolicy,
    const MObject &skinClusterObject,
    const MDagPathArray &requiredInfluencePaths,
    MDagPathArray &resolvedInfluencePaths);

MStatus BuildPhysicalInfluenceIndices(
    const MDagPathArray &influencePaths,
    MIntArray &influenceIndices);

MStatus PrepareSkinClusterForSetWeights(
    const MObject &skinClusterObject,
    unsigned int maxAssignedInfluences);

MStatus SetSkinClusterBindPreMatrix(
    const MObject &skinClusterObject,
    unsigned int logicalInfluenceIndex,
    const MMatrix &bindPreMatrix);

MStatus SetSkinClusterGeomMatrix(
    const MObject &skinClusterObject,
    const MMatrix &geomMatrix);

MStatus CreateMeshVertexComponent(
    unsigned int vertexCount,
    MObject &vertexComponent);

unsigned int NormalizeWeightBufferInPlace(
    MFloatArray &weights,
    unsigned int vertexCount,
    unsigned int influenceCount);

} // namespace dcc_skinning
