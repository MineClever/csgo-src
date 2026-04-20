#include "SmdSceneExporter.h"

#include <common/ExportAnimationUtils.h>
#include <common/MaterialExportUtils.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
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
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

namespace smd_export_impl
{
constexpr const char *kSmdMaterialNameAttribute = "mayaSmdMaterialName";

std::string DagPathKey(const MDagPath &dagPath)
{
    return dagPath.fullPathName().asChar();
}

}

SmdSceneExporter::SmdSceneExporter(
    MPxFileTranslator::FileAccessMode mode,
    const dcc_export_transform::ExportTransformPolicy &transformPolicy,
    bool exportMesh,
    bool exportAnimation)
    : mode_(mode)
    , transformPolicy_(transformPolicy)
    , exportMesh_(exportMesh)
    , exportAnimation_(exportAnimation)
{
}

MStatus SmdSceneExporter::Build()
{
    document_ = simple_smd::Document();
    document_.version = 1;
    exportRoots_.clear();
    meshRoots_.clear();
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

    if (exportAnimation_)
    {
        status = buildSkeleton();
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    if (exportMesh_)
    {
        status = buildTriangles();
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return applyDocumentTransformCorrection();
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
            auto parentIt = nodeIndexByPath_.find(smd_export_impl::DagPathKey(parentPath));
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

    if (isImportWrapperRoot(dagPath))
    {
        return true;
    }

    if (dagPath.hasFn(MFn::kTransform) && hasRenderableMeshDescendant(dagPath))
    {
        return true;
    }

    return shouldExportNode(dagPath);
}

bool SmdSceneExporter::shouldExportNode(const MDagPath &dagPath) const
{
    if (dagPath.hasFn(MFn::kJoint))
    {
        return true;
    }

    if (!dagPath.hasFn(MFn::kTransform))
    {
        return false;
    }

    if (isImportWrapperRoot(dagPath) || hasRenderableMeshDescendant(dagPath))
    {
        return false;
    }

    return true;
}

bool SmdSceneExporter::isImportWrapperRoot(const MDagPath &dagPath) const
{
    if (!dagPath.isValid() || !dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint))
    {
        return false;
    }

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return false;
    }

    const MString nodeName = dagNode.name();
    return nodeName.indexW("smd_import_root") == 0;
}

bool SmdSceneExporter::hasRenderableMeshDescendant(const MDagPath &dagPath) const
{
    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return false;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        const MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            continue;
        }

        MFnDagNode childDagNode(childObject, &status);
        if (status && !childDagNode.isIntermediateObject())
        {
            return true;
        }

        if (childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint))
        {
            MDagPath childPath = dagPath;
            childPath.push(childObject);
            if (hasRenderableMeshDescendant(childPath))
            {
                return true;
            }
        }
    }

    return false;
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

                if (isImportWrapperRoot(dagPath))
                {
                    MStatus status;
                    MFnDagNode dagNode(dagPath, &status);
                    if (!status)
                    {
                        continue;
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
                        if (shouldExportNode(childPath))
                        {
                            exportRoots_.push_back(childPath);
                        }
                    }
                    meshRoots_.push_back(dagPath);
                }
                else if (shouldExportRoot(dagPath))
                {
                    exportRoots_.push_back(dagPath);
                    meshRoots_.push_back(dagPath);
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
                meshRoots_.push_back(dagPath);
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
    const bool exportCurrentNode = shouldExportNode(dagPath);
    const std::string pathKey = smd_export_impl::DagPathKey(dagPath);
    if (exportCurrentNode && nodeIndexByPath_.find(pathKey) == nodeIndexByPath_.end())
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
        status = collectNodesRecursive(childPath, exportCurrentNode ? nodeIndexByPath_[pathKey] : -1);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneExporter::buildSkeleton()
{
    document_.skeletonFrames.clear();

    std::vector<double> frameTimes;
    collectAnimationFrameTimes(frameTimes);
    if (frameTimes.empty())
    {
        frameTimes.push_back(0.0);
    }
    std::sort(frameTimes.begin(), frameTimes.end());

    dcc_animation_export::CurveCache curveCache;
    std::vector<dcc_animation_export::TransformSampleSet> transformSamplesByNode(exportNodes_.size());
    for (size_t nodeIndex = 0; nodeIndex < exportNodes_.size(); ++nodeIndex)
    {
        MStatus status;
        MFnDependencyNode nodeFn(exportNodes_[nodeIndex].node(), &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        if (!dcc_animation_export::BuildTransformSampleSet(nodeFn, transformSamplesByNode[nodeIndex], &curveCache))
        {
            return MStatus::kFailure;
        }
    }

    for (double frameTime : frameTimes)
    {
        simple_smd::SkeletonFrame frame;
        frame.time = static_cast<int>(std::lround(frameTime));
        frame.poses.reserve(exportNodes_.size());

        for (size_t nodeIndex = 0; nodeIndex < exportNodes_.size(); ++nodeIndex)
        {
            simple_smd::SkeletonPose pose;
            pose.boneIndex = static_cast<int>(nodeIndex);
            const std::array<double, 3> translationValues =
                dcc_animation_export::EvaluateSampleSetValues(transformSamplesByNode[nodeIndex].translation, frameTime, MTime::uiUnit());
            const std::array<double, 3> rotationValues =
                dcc_animation_export::EvaluateSampleSetValues(transformSamplesByNode[nodeIndex].rotation, frameTime, MTime::uiUnit());
            pose.tx = translationValues[0];
            pose.ty = translationValues[1];
            pose.tz = translationValues[2];
            pose.rx = rotationValues[0];
            pose.ry = rotationValues[1];
            pose.rz = rotationValues[2];
            frame.poses.push_back(pose);
        }

        document_.skeletonFrames.push_back(frame);
    }

    return MS::kSuccess;
}

void SmdSceneExporter::collectAnimationFrameTimes(std::vector<double> &frameTimes) const
{
    frameTimes.clear();
    dcc_animation_export::CurveCache curveCache;

    for (const MDagPath &dagPath : exportNodes_)
    {
        MStatus status;
        MFnDependencyNode nodeFn(dagPath.node(), &status);
        if (!status)
        {
            continue;
        }

        dcc_animation_export::TransformSampleSet transformSamples;
        if (dcc_animation_export::BuildTransformSampleSet(nodeFn, transformSamples, &curveCache))
        {
            dcc_animation_export::AppendTransformSampleTimes(transformSamples, frameTimes, MTime::uiUnit());
        }
    }
}

int SmdSceneExporter::findOwningNodeIndex(const MDagPath &dagPath) const
{
    MDagPath currentPath = dagPath;
    while (currentPath.length() > 0)
    {
        auto nodeIt = nodeIndexByPath_.find(smd_export_impl::DagPathKey(currentPath));
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

    for (const MDagPath &rootPath : meshRoots_)
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

            const std::string meshKey = smd_export_impl::DagPathKey(meshPath);
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

            dcc_material_export::MeshShadingAssignments shadingAssignments;
            dcc_material_export::GetMeshShadingAssignments(meshFn, meshPath.instanceNumber(), shadingAssignments);

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
                if (!dcc_material_export::ReadStringAttribute(meshPath.node(), smd_export_impl::kSmdMaterialNameAttribute, materialName))
                {
                    MDagPath meshParentPath = meshPath;
                    meshParentPath.pop();
                    dcc_material_export::ReadStringAttribute(meshParentPath.node(), smd_export_impl::kSmdMaterialNameAttribute, materialName);
                }
                MObject shadingGroupObject;
                if (dcc_material_export::TryGetAssignedShadingGroup(shadingAssignments, polygonIt.index(), shadingGroupObject))
                {
                    const dcc_material_export::ShadingGroupMaterialInfo materialInfo =
                        dcc_material_export::DescribeShadingGroupMaterial(shadingGroupObject, "defaultMaterial");
                    materialName = dcc_material_export::ResolvePreferredMaterialName(materialName, "defaultMaterial", materialInfo);
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

MStatus SmdSceneExporter::applyDocumentTransformCorrection()
{
    if (transformPolicy_.IsIdentity())
    {
        return MS::kSuccess;
    }

    for (simple_smd::SkeletonFrame &frame : document_.skeletonFrames)
    {
        for (simple_smd::SkeletonPose &pose : frame.poses)
        {
            if (pose.boneIndex < 0 || static_cast<size_t>(pose.boneIndex) >= document_.nodes.size())
            {
                continue;
            }

            const simple_smd::Node &node = document_.nodes[pose.boneIndex];
            MVector correctedTranslation = dcc_export_transform::ApplyToLocalTranslation(
                transformPolicy_,
                MVector(pose.tx, pose.ty, pose.tz));
            MEulerRotation correctedRotation(pose.rx, pose.ry, pose.rz);

            if (node.parentIndex < 0)
            {
                correctedTranslation = dcc_export_transform::ApplyToTopLevelTranslation(
                    transformPolicy_,
                    MVector(pose.tx, pose.ty, pose.tz));
                correctedRotation = dcc_export_transform::ApplyToTopLevelEulerRotation(
                    transformPolicy_,
                    correctedRotation);
            }

            pose.tx = correctedTranslation.x;
            pose.ty = correctedTranslation.y;
            pose.tz = correctedTranslation.z;
            pose.rx = correctedRotation.x;
            pose.ry = correctedRotation.y;
            pose.rz = correctedRotation.z;
        }
    }

    for (simple_smd::Triangle &triangle : document_.triangles)
    {
        for (simple_smd::TriangleVertex &vertex : triangle.vertices)
        {
            const MVector correctedPoint = dcc_export_transform::ApplyToBakedMeshPoint(
                transformPolicy_,
                MVector(vertex.px, vertex.py, vertex.pz));
            const MVector correctedNormal = dcc_export_transform::ApplyToBakedMeshNormal(
                transformPolicy_,
                MVector(vertex.nx, vertex.ny, vertex.nz));

            vertex.px = correctedPoint.x;
            vertex.py = correctedPoint.y;
            vertex.pz = correctedPoint.z;
            vertex.nx = correctedNormal.x;
            vertex.ny = correctedNormal.y;
            vertex.nz = correctedNormal.z;
        }
    }

    for (simple_smd::VertexAnimationFrame &frame : document_.vertexAnimationFrames)
    {
        for (simple_smd::VertexAnimationSample &sample : frame.samples)
        {
            const MVector correctedPoint = dcc_export_transform::ApplyToBakedMeshPoint(
                transformPolicy_,
                MVector(sample.px, sample.py, sample.pz));
            const MVector correctedNormal = dcc_export_transform::ApplyToBakedMeshNormal(
                transformPolicy_,
                MVector(sample.nx, sample.ny, sample.nz));

            sample.px = correctedPoint.x;
            sample.py = correctedPoint.y;
            sample.pz = correctedPoint.z;
            sample.nx = correctedNormal.x;
            sample.ny = correctedNormal.y;
            sample.nz = correctedNormal.z;
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
        auto nodeIt = nodeIndexByPath_.find(smd_export_impl::DagPathKey(influencePaths[influenceIndex]));
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
