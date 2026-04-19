#include "SkinClusterUtils.h"

#include "MayaCommandUtils.h"

#include <algorithm>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MPlug.h>

namespace dcc_skinning
{

std::unordered_map<std::string, unsigned int> BuildInfluenceIndexByPath(const MDagPathArray &influencePaths)
{
    std::unordered_map<std::string, unsigned int> influenceByPath;
    for (unsigned int index = 0; index < influencePaths.length(); ++index)
    {
        influenceByPath[influencePaths[index].fullPathName().asChar()] = index;
    }
    return influenceByPath;
}

bool FindMatchingInfluencePath(
    const dcc_import_policy::SceneImportPolicy &scenePolicy,
    const MDagPathArray &influencePaths,
    const MDagPath &requiredInfluencePath,
    MDagPath &matchedInfluencePath)
{
    const std::string requiredFullPath = requiredInfluencePath.fullPathName().asChar();
    const std::string requiredLeafName = requiredInfluencePath.partialPathName().asChar();
    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        const MDagPath &candidatePath = influencePaths[influenceIndex];
        if (requiredFullPath == candidatePath.fullPathName().asChar())
        {
            matchedInfluencePath = candidatePath;
            return true;
        }

        if (dcc_import_policy::MatchesNodeNameForAppend(
            scenePolicy,
            candidatePath.partialPathName().asChar(),
            requiredLeafName))
        {
            matchedInfluencePath = candidatePath;
            return true;
        }
    }

    return false;
}

MStatus EnsureSkinClusterContainsInfluences(
    const dcc_import_policy::SceneImportPolicy &scenePolicy,
    const MObject &skinClusterObject,
    const MDagPathArray &requiredInfluencePaths,
    MDagPathArray &resolvedInfluencePaths)
{
    resolvedInfluencePaths.clear();
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    skinClusterFn.influenceObjects(resolvedInfluencePaths, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < requiredInfluencePaths.length(); ++influenceIndex)
    {
        MDagPath matchedInfluencePath;
        if (FindMatchingInfluencePath(scenePolicy, resolvedInfluencePaths, requiredInfluencePaths[influenceIndex], matchedInfluencePath))
        {
            continue;
        }

        status = maya_cmd::AddSkinClusterInfluence(skinClusterNode.name(), requiredInfluencePaths[influenceIndex]);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    resolvedInfluencePaths.clear();
    skinClusterFn.influenceObjects(resolvedInfluencePaths, &status);
    return status ? MS::kSuccess : MS::kFailure;
}

MStatus BuildPhysicalInfluenceIndices(
    const MDagPathArray &influencePaths,
    MIntArray &influenceIndices)
{
    influenceIndices.clear();
    for (unsigned int influencePathIndex = 0; influencePathIndex < influencePaths.length(); ++influencePathIndex)
    {
        influenceIndices.append(static_cast<int>(influencePathIndex));
    }

    return MS::kSuccess;
}

MStatus PrepareSkinClusterForSetWeights(
    const MObject &skinClusterObject,
    unsigned int maxAssignedInfluences)
{
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug maintainMaxInfluencesPlug = skinClusterNode.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        maintainMaxInfluencesPlug.setBool(false);
    }
    status = MS::kSuccess;

    MPlug normalizeWeightsPlug = skinClusterNode.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        normalizeWeightsPlug.setShort(0);
    }
    status = MS::kSuccess;

    MPlug maxInfluencesPlug = skinClusterNode.findPlug("maxInfluences", true, &status);
    if (status && !maxInfluencesPlug.isNull())
    {
        maxInfluencesPlug.setInt(static_cast<int>(std::max(1u, maxAssignedInfluences)));
    }

    return MS::kSuccess;
}

MStatus SetSkinClusterBindPreMatrix(
    const MObject &skinClusterObject,
    unsigned int logicalInfluenceIndex,
    const MMatrix &bindPreMatrix)
{
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug bindPreMatrixPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    bindPreMatrixPlug = bindPreMatrixPlug.elementByLogicalIndex(logicalInfluenceIndex, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnMatrixData matrixDataFn;
    MObject bindPreMatrixObject = matrixDataFn.create(bindPreMatrix, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return bindPreMatrixPlug.setMObject(bindPreMatrixObject);
}

MStatus SetSkinClusterGeomMatrix(
    const MObject &skinClusterObject,
    const MMatrix &geomMatrix)
{
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnMatrixData geomMatrixDataFn;
    MObject geomMatrixObject = geomMatrixDataFn.create(geomMatrix, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return geomMatrixPlug.setMObject(geomMatrixObject);
}

MStatus CreateMeshVertexComponent(
    unsigned int vertexCount,
    MObject &vertexComponent)
{
    vertexComponent = MObject::kNullObj;

    MStatus status;
    MFnSingleIndexedComponent componentFn;
    vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }

    return componentFn.addElements(vertexIds);
}

unsigned int NormalizeWeightBufferInPlace(
    MFloatArray &weights,
    unsigned int vertexCount,
    unsigned int influenceCount)
{
    if (vertexCount == 0 || influenceCount == 0)
    {
        return 0;
    }

    unsigned int maxAssignedInfluenceCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        float totalWeight = 0.0f;
        unsigned int assignedInfluenceCount = 0;
        for (unsigned int influenceSlot = 0; influenceSlot < influenceCount; ++influenceSlot)
        {
            const float weightValue = weights[vertexIndex * influenceCount + influenceSlot];
            totalWeight += weightValue;
            if (weightValue > 1.0e-6f)
            {
                ++assignedInfluenceCount;
            }
        }
        maxAssignedInfluenceCount = std::max(maxAssignedInfluenceCount, assignedInfluenceCount);

        if (totalWeight > 1.0e-6f)
        {
            const float invTotalWeight = 1.0f / totalWeight;
            for (unsigned int influenceSlot = 0; influenceSlot < influenceCount; ++influenceSlot)
            {
                weights[vertexIndex * influenceCount + influenceSlot] *= invTotalWeight;
            }
        }
    }

    return maxAssignedInfluenceCount;
}

} // namespace dcc_skinning
