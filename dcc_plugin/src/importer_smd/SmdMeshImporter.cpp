#include "SmdMeshImporter.h"

#include "../common_smd/MayaSmdCommon.h"

#include <cctype>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>

#include <maya/MDagPathArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

namespace
{
std::string SanitizeMeshName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("smd_mesh") : value;
}
}

SmdMeshImporter::SmdMeshImporter(
    const simple_smd::Document &document,
    const std::unordered_map<int, MDagPath> &jointPathsByBone)
    : document_(document)
    , jointPathsByBone_(jointPathsByBone)
{
}

MStatus SmdMeshImporter::Import(MObject parent) const
{
    if (document_.triangles.empty())
    {
        return MS::kSuccess;
    }

    std::vector<std::string> materialNames;
    materialNames.reserve(document_.triangles.size());
    for (const simple_smd::Triangle &triangle : document_.triangles)
    {
        if (std::find(materialNames.begin(), materialNames.end(), triangle.materialName) == materialNames.end())
        {
            materialNames.push_back(triangle.materialName);
        }
    }

    for (const std::string &materialName : materialNames)
    {
        const MStatus status = importMaterialGroup(materialName, parent);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdMeshImporter::importMaterialGroup(const std::string &materialName, MObject parent) const
{
    MPointArray points;
    MIntArray polygonCounts;
    MIntArray polygonConnects;
    MFloatArray uValues;
    MFloatArray vValues;
    MIntArray uvIds;
    MIntArray normalFaceIds;
    MIntArray normalVertexIds;
    MVectorArray normals;
    std::vector<std::vector<simple_smd::TriangleWeight>> vertexLinks;

    bool hasWeights = false;
    int nextVertexIndex = 0;
    int faceIndex = 0;
    for (const simple_smd::Triangle &triangle : document_.triangles)
    {
        if (triangle.materialName != materialName || triangle.vertices.size() != 3)
        {
            continue;
        }

        polygonCounts.append(3);
        for (int vertexInFace = 0; vertexInFace < 3; ++vertexInFace)
        {
            const simple_smd::TriangleVertex &vertex = triangle.vertices[vertexInFace];
            points.append(vertex.px, vertex.py, vertex.pz);
            polygonConnects.append(nextVertexIndex);
            uValues.append(static_cast<float>(vertex.u));
            vValues.append(static_cast<float>(1.0 - vertex.v));
            uvIds.append(nextVertexIndex);
            normals.append(MVector(vertex.nx, vertex.ny, vertex.nz));
            normalFaceIds.append(faceIndex);
            normalVertexIds.append(vertexInFace);
            vertexLinks.push_back(vertex.links);
            hasWeights = hasWeights || !vertex.links.empty();
            ++nextVertexIndex;
        }
        ++faceIndex;
    }

    if (points.length() == 0 || polygonCounts.length() == 0)
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform transformFn;
    const MObject transformObject = transformFn.create(parent, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    transformFn.setName((SanitizeMeshName(materialName) + "_grp#").c_str());

    MFnMesh meshFn;
    const MObject meshObject = meshFn.create(points.length(), polygonCounts.length(), points, polygonCounts, polygonConnects, transformObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    meshFn.setName((SanitizeMeshName(materialName) + "Shape#").c_str());

    status = meshFn.setUVs(uValues, vValues);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = meshFn.assignUVs(polygonCounts, uvIds);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = meshFn.setFaceVertexNormals(normals, normalFaceIds, normalVertexIds);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MDagPath meshTransformPath;
    status = MDagPath::getAPathTo(transformObject, meshTransformPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (hasWeights)
    {
        status = applySkinning(vertexLinks, meshTransformPath);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdMeshImporter::applySkinning(
    const std::vector<std::vector<simple_smd::TriangleWeight>> &vertexLinks,
    const MDagPath &meshTransformPath) const
{
    std::vector<int> activeBoneIndices;
    for (const auto &links : vertexLinks)
    {
        for (const simple_smd::TriangleWeight &weight : links)
        {
            if (weight.weight <= 0.0)
            {
                continue;
            }

            if (jointPathsByBone_.find(weight.boneIndex) == jointPathsByBone_.end())
            {
                continue;
            }

            if (std::find(activeBoneIndices.begin(), activeBoneIndices.end(), weight.boneIndex) == activeBoneIndices.end())
            {
                activeBoneIndices.push_back(weight.boneIndex);
            }
        }
    }

    if (activeBoneIndices.empty())
    {
        return MS::kSuccess;
    }

    MString command("skinCluster -tsb");
    for (int boneIndex : activeBoneIndices)
    {
        const auto jointIt = jointPathsByBone_.find(boneIndex);
        if (jointIt == jointPathsByBone_.end())
        {
            continue;
        }

        command += " \"";
        command += jointIt->second.fullPathName();
        command += "\"";
    }
    command += " \"";
    command += meshTransformPath.fullPathName();
    command += "\"";

    MString skinClusterName;
    MStatus status = MGlobal::executeCommand(command, skinClusterName);
    if (!status || skinClusterName.length() == 0)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to create skinCluster for ") + meshTransformPath.fullPathName(), status);
    }

    MSelectionList selection;
    status = selection.add(skinClusterName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MObject skinClusterObject;
    status = selection.getDependNode(0, skinClusterObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MDagPathArray influencePaths;
    skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MIntArray influenceIndices;
    std::unordered_map<int, unsigned int> boneToInfluenceSlot;
    for (unsigned int influencePathIndex = 0; influencePathIndex < influencePaths.length(); ++influencePathIndex)
    {
        const unsigned int influenceIndex = skinClusterFn.indexForInfluenceObject(influencePaths[influencePathIndex], &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        for (int boneIndex : activeBoneIndices)
        {
            const auto jointIt = jointPathsByBone_.find(boneIndex);
            if (jointIt == jointPathsByBone_.end())
            {
                continue;
            }

            if (jointIt->second.fullPathName() == influencePaths[influencePathIndex].fullPathName())
            {
                boneToInfluenceSlot[boneIndex] = influenceIndices.length();
                influenceIndices.append(static_cast<int>(influenceIndex));
                break;
            }
        }
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MSelectionList meshSelection;
    status = meshSelection.add(meshTransformPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MDagPath meshDagPath;
    status = meshSelection.getDagPath(0, meshDagPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexLinks.size(); ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFloatArray weights;
    weights.setLength(static_cast<unsigned int>(vertexLinks.size()) * influenceIndices.length());
    for (unsigned int weightIndex = 0; weightIndex < weights.length(); ++weightIndex)
    {
        weights[weightIndex] = 0.0f;
    }

    for (unsigned int vertexIndex = 0; vertexIndex < vertexLinks.size(); ++vertexIndex)
    {
        float totalWeight = 0.0f;
        for (const simple_smd::TriangleWeight &weight : vertexLinks[vertexIndex])
        {
            const auto influenceSlotIt = boneToInfluenceSlot.find(weight.boneIndex);
            if (influenceSlotIt == boneToInfluenceSlot.end() || weight.weight <= 0.0)
            {
                continue;
            }

            const unsigned int influenceSlot = influenceSlotIt->second;
            weights[vertexIndex * influenceIndices.length() + influenceSlot] += static_cast<float>(weight.weight);
            totalWeight += static_cast<float>(weight.weight);
        }

        if (totalWeight > 1.0e-6f)
        {
            const float invTotalWeight = 1.0f / totalWeight;
            for (unsigned int influenceSlot = 0; influenceSlot < influenceIndices.length(); ++influenceSlot)
            {
                weights[vertexIndex * influenceIndices.length() + influenceSlot] *= invTotalWeight;
            }
        }
    }

    status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}
