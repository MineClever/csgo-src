#include "SmdSceneExporter.h"

#include "../common_smd/MayaSmdCommon.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>

#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MDoubleArray.h>
#include <maya/MPointArray.h>
#include <maya/MObjectArray.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>

namespace
{
std::string DagPathKey(const MDagPath &dagPath)
{
    return dagPath.fullPathName().asChar();
}
}

SmdSceneExporter::SmdSceneExporter(MPxFileTranslator::FileAccessMode mode)
    : mode_(mode)
{
}

MStatus SmdSceneExporter::Build()
{
    document_ = simple_smd::Document();
    document_.version = 1;
    exportRoots_.clear();
    exportNodes_.clear();
    nodeIndexByPath_.clear();

    MStatus status = collectExportRoots();
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (exportRoots_.empty())
    {
        return maya_smd::ReportError("maya_smd: nothing to export.");
    }

    for (const MDagPath &rootPath : exportRoots_)
    {
        status = collectNodesRecursive(rootPath, -1);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    status = buildNodes();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = buildSkeleton();
    if (!status)
    {
        return MStatus::kFailure;
    }

    return buildTriangles();
}

const simple_smd::Document &SmdSceneExporter::document() const
{
    return document_;
}

MStatus SmdSceneExporter::buildNodes()
{
    MStatus status;
    document_.nodes.clear();
    document_.nodes.reserve(exportNodes_.size());
    for (size_t nodeIndex = 0; nodeIndex < exportNodes_.size(); ++nodeIndex)
    {
        MFnDagNode dagNode(exportNodes_[nodeIndex], &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        simple_smd::Node node;
        node.index = static_cast<int>(nodeIndex);
        node.name = dagNode.name().asChar();

        MDagPath parentPath = exportNodes_[nodeIndex];
        if (parentPath.length() > 0)
        {
            parentPath.pop();
            auto parentIt = nodeIndexByPath_.find(DagPathKey(parentPath));
            node.parentIndex = parentIt == nodeIndexByPath_.end() ? -1 : parentIt->second;
        }

        document_.nodes.push_back(node);
    }

    return MS::kSuccess;
}

bool SmdSceneExporter::shouldExportRoot(const MDagPath &dagPath) const
{
    if (!dagPath.isValid())
    {
        return false;
    }

    MFnDagNode dagNode(dagPath);
    if (dagNode.isIntermediateObject())
    {
        return false;
    }

    return shouldExportNode(dagPath);
}

bool SmdSceneExporter::shouldExportNode(const MDagPath &dagPath) const
{
    return dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint);
}

MStatus SmdSceneExporter::collectExportRoots()
{
    if (mode_ == MPxFileTranslator::kExportActiveAccessMode)
    {
        MSelectionList activeSelection;
        if (MGlobal::getActiveSelectionList(activeSelection) == MS::kSuccess)
        {
            for (unsigned int index = 0; index < activeSelection.length(); ++index)
            {
                MDagPath dagPath;
                if (activeSelection.getDagPath(index, dagPath) != MS::kSuccess)
                {
                    continue;
                }

                if (dagPath.hasFn(MFn::kMesh))
                {
                    dagPath.pop();
                }

                if (shouldExportRoot(dagPath))
                {
                    exportRoots_.push_back(dagPath);
                }
            }
        }
    }

    if (exportRoots_.empty())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            if (dagIterator.depth() != 1)
            {
                continue;
            }

            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) == MS::kSuccess && shouldExportRoot(dagPath))
            {
                exportRoots_.push_back(dagPath);
            }
        }
    }

    std::vector<MDagPath> filteredRoots;
    for (const MDagPath &candidate : exportRoots_)
    {
        bool isDescendant = false;
        for (const MDagPath &other : exportRoots_)
        {
            if (candidate == other)
            {
                continue;
            }

            const MString candidatePath = candidate.fullPathName();
            const MString otherPath = other.fullPathName();
            if (candidate.length() > other.length() && candidatePath.indexW(otherPath) == 0)
            {
                isDescendant = true;
                break;
            }
        }

        if (!isDescendant)
        {
            filteredRoots.push_back(candidate);
        }
    }

    exportRoots_ = filteredRoots;
    return MS::kSuccess;
}

MStatus SmdSceneExporter::collectNodesRecursive(const MDagPath &dagPath, int)
{
    if (!shouldExportNode(dagPath))
    {
        return MS::kSuccess;
    }

    const std::string pathKey = DagPathKey(dagPath);
    if (nodeIndexByPath_.find(pathKey) == nodeIndexByPath_.end())
    {
        nodeIndexByPath_[pathKey] = static_cast<int>(exportNodes_.size());
        exportNodes_.push_back(dagPath);
    }

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        const MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        status = collectNodesRecursive(childPath, nodeIndexByPath_[pathKey]);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneExporter::buildSkeleton()
{
    simple_smd::SkeletonFrame bindFrame;
    bindFrame.time = 0;
    bindFrame.poses.reserve(exportNodes_.size());

    for (size_t nodeIndex = 0; nodeIndex < exportNodes_.size(); ++nodeIndex)
    {
        MStatus status;
        MFnTransform transformFn(exportNodes_[nodeIndex], &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        const MVector translation = transformFn.translation(MSpace::kTransform, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MEulerRotation rotation;
        status = transformFn.getRotation(rotation);
        if (!status)
        {
            return MStatus::kFailure;
        }

        simple_smd::SkeletonPose pose;
        pose.boneIndex = static_cast<int>(nodeIndex);
        pose.tx = translation.x;
        pose.ty = translation.y;
        pose.tz = translation.z;
        pose.rx = rotation.x;
        pose.ry = rotation.y;
        pose.rz = rotation.z;
        bindFrame.poses.push_back(pose);
    }

    document_.skeletonFrames.clear();
    document_.skeletonFrames.push_back(bindFrame);
    return MS::kSuccess;
}

int SmdSceneExporter::findOwningNodeIndex(const MDagPath &dagPath) const
{
    MDagPath currentPath = dagPath;
    while (currentPath.length() > 0)
    {
        auto nodeIt = nodeIndexByPath_.find(DagPathKey(currentPath));
        if (nodeIt != nodeIndexByPath_.end())
        {
            return nodeIt->second;
        }
        currentPath.pop();
    }

    return exportNodes_.empty() ? 0 : 0;
}

MStatus SmdSceneExporter::buildTriangles()
{
    std::unordered_set<std::string> visitedMeshes;
    document_.triangles.clear();

    for (const MDagPath &rootPath : exportRoots_)
    {
        MItDag dagIterator(MItDag::kDepthFirst, MFn::kMesh);
        dagIterator.reset(rootPath, MItDag::kDepthFirst, MFn::kMesh);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            MDagPath meshPath;
            if (dagIterator.getPath(meshPath) != MS::kSuccess)
            {
                continue;
            }

            MStatus status;
            MFnDagNode meshDagNode(meshPath, &status);
            if (!status || meshDagNode.isIntermediateObject())
            {
                continue;
            }

            const std::string meshKey = DagPathKey(meshPath);
            if (!visitedMeshes.insert(meshKey).second)
            {
                continue;
            }

            MFnMesh meshFn(meshPath, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }

            std::unordered_map<int, std::vector<simple_smd::TriangleWeight>> skinWeightsByVertex;
            status = collectSkinWeights(meshPath, skinWeightsByVertex);
            if (!status)
            {
                return MStatus::kFailure;
            }

            MObjectArray shadingGroups;
            MIntArray faceShaderIndices;
            meshFn.getConnectedShaders(meshPath.instanceNumber(), shadingGroups, faceShaderIndices);

            MObject component = MObject::kNullObj;
            MItMeshPolygon polygonIt(meshPath, component, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }

            const int parentBoneIndex = findOwningNodeIndex(meshPath);
            for (; !polygonIt.isDone(); polygonIt.next())
            {
                MPointArray trianglePoints;
                MIntArray triangleVertexIndices;
                status = polygonIt.getTriangles(trianglePoints, triangleVertexIndices, MSpace::kObject);
                if (!status)
                {
                    return MStatus::kFailure;
                }

                std::string materialName = "defaultMaterial";
                if (polygonIt.index() < static_cast<int>(faceShaderIndices.length()))
                {
                    const int shaderIndex = faceShaderIndices[polygonIt.index()];
                    if (shaderIndex >= 0 && shaderIndex < static_cast<int>(shadingGroups.length()))
                    {
                        MFnDependencyNode shaderNode(shadingGroups[shaderIndex], &status);
                        if (status)
                        {
                            materialName = shaderNode.name().asChar();
                        }
                    }
                }

                const unsigned int triangleCount = triangleVertexIndices.length() / 3;
                for (unsigned int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
                {
                    simple_smd::Triangle triangle;
                    triangle.materialName = materialName;

                    for (unsigned int vertexInTriangle = 0; vertexInTriangle < 3; ++vertexInTriangle)
                    {
                        const int vertexIndex = triangleVertexIndices[triangleIndex * 3 + vertexInTriangle];

                        simple_smd::TriangleVertex vertex;
                        vertex.parentBoneIndex = parentBoneIndex;

                        MPoint point;
                        status = meshFn.getPoint(vertexIndex, point, MSpace::kObject);
                        if (!status)
                        {
                            return MStatus::kFailure;
                        }
                        vertex.px = point.x;
                        vertex.py = point.y;
                        vertex.pz = point.z;

                        MVector normal;
                        status = polygonIt.getNormal(static_cast<int>(vertexInTriangle), normal, MSpace::kObject);
                        if (!status)
                        {
                            return MStatus::kFailure;
                        }
                        vertex.nx = normal.x;
                        vertex.ny = normal.y;
                        vertex.nz = normal.z;

                        float2 uv{};
                        if (polygonIt.hasUVs() && polygonIt.getUV(static_cast<int>(vertexInTriangle), uv) == MS::kSuccess)
                        {
                            vertex.u = uv[0];
                            vertex.v = 1.0 - uv[1];
                        }

                        auto weightIt = skinWeightsByVertex.find(vertexIndex);
                        if (weightIt != skinWeightsByVertex.end())
                        {
                            vertex.links = weightIt->second;
                        }

                        triangle.vertices.push_back(vertex);
                    }

                    document_.triangles.push_back(triangle);
                }
            }
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneExporter::collectSkinWeights(
    const MDagPath &meshPath,
    std::unordered_map<int, std::vector<simple_smd::TriangleWeight>> &weightsByVertex) const
{
    weightsByVertex.clear();

    MStatus status;
    MObject meshNodeCopy(meshPath.node());
    MItDependencyGraph dgIt(
        meshNodeCopy,
        MFn::kSkinClusterFilter,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status || dgIt.isDone())
    {
        return MS::kSuccess;
    }

    const MObject skinClusterObject = dgIt.currentItem(&status);
    if (!status || skinClusterObject.isNull())
    {
        return MS::kSuccess;
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnMesh meshFn(meshPath, &status);
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

    const unsigned int vertexCount = meshFn.numVertices(&status);
    if (!status || vertexCount == 0)
    {
        return MS::kSuccess;
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MDagPathArray influencePaths;
    const unsigned int influenceCount = skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status || influenceCount == 0)
    {
        return MS::kSuccess;
    }

    std::vector<int> influenceToBoneIndex(influenceCount, -1);
    for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
    {
        auto nodeIt = nodeIndexByPath_.find(DagPathKey(influencePaths[influenceIndex]));
        if (nodeIt != nodeIndexByPath_.end())
        {
            influenceToBoneIndex[influenceIndex] = nodeIt->second;
        }
    }

    MDoubleArray weights;
    unsigned int exportedInfluenceCount = 0;
    status = skinClusterFn.getWeights(meshPath, vertexComponent, weights, exportedInfluenceCount);
    if (!status || exportedInfluenceCount != influenceCount)
    {
        return MStatus::kFailure;
    }

    constexpr double kWeightEpsilon = 1.0e-5;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const unsigned int baseOffset = vertexIndex * influenceCount;
        std::vector<simple_smd::TriangleWeight> vertexWeights;
        double totalWeight = 0.0;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            const int boneIndex = influenceToBoneIndex[influenceIndex];
            const double weightValue = weights[baseOffset + influenceIndex];
            if (boneIndex < 0 || std::abs(weightValue) <= kWeightEpsilon)
            {
                continue;
            }

            simple_smd::TriangleWeight weight;
            weight.boneIndex = boneIndex;
            weight.weight = weightValue;
            vertexWeights.push_back(weight);
            totalWeight += weightValue;
        }

        std::sort(vertexWeights.begin(), vertexWeights.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.weight != rhs.weight)
            {
                return lhs.weight > rhs.weight;
            }
            return lhs.boneIndex < rhs.boneIndex;
        });

        if (totalWeight > kWeightEpsilon)
        {
            const double invTotalWeight = 1.0 / totalWeight;
            for (simple_smd::TriangleWeight &weight : vertexWeights)
            {
                weight.weight *= invTotalWeight;
            }
        }

        if (!vertexWeights.empty())
        {
            weightsByVertex[static_cast<int>(vertexIndex)] = std::move(vertexWeights);
        }
    }

    return MS::kSuccess;
}
