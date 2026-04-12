#include "SmdMeshImporter.h"

#include <common/MayaCommandUtils.h>
#include <common/ImportTransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <cctype>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>

#include <maya/MDagPathArray.h>
#include <maya/MDGModifier.h>
#include <maya/MFloatArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTransform.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

namespace smd_mesh_import_impl
{
constexpr const char *kSmdMaterialNameAttribute = "mayaSmdMaterialName";

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

MStatus SetStringAttribute(MObject nodeObject, const char *attributeName, const std::string &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject attributeObject = nodeFn.attribute(attributeName, &status);
    if (!status)
    {
        MFnTypedAttribute attributeFn;
        MFnStringData stringDataFn;
        MObject defaultValue = stringDataFn.create("", &status);
        if (!status)
        {
            return MS::kFailure;
        }

        attributeObject = attributeFn.create(attributeName, attributeName, MFnData::kString, defaultValue, &status);
        if (!status)
        {
            return MS::kFailure;
        }
        attributeFn.setWritable(true);
        attributeFn.setStorable(true);
        attributeFn.setReadable(true);
        attributeFn.setKeyable(false);

        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return attributePlug.setString(value.c_str());
}

bool ShouldDeleteMeshHistoryNodeType(const MString &typeName)
{
    return typeName == "skinCluster" ||
        typeName == "blendShape" ||
        typeName == "tweak" ||
        typeName == "dagPose";
}

bool DependencyNodeExists(const MString &nodeName)
{
    MSelectionList selection;
    return selection.add(nodeName) == MS::kSuccess;
}

MStatus DeleteExistingMeshGroupForUpdate(const dcc_import_policy::SceneImportPolicy &scenePolicy, const MObject &transformObject)
{
    if (!dcc_import_policy::UsesUpdateCurrentScene(scenePolicy))
    {
        return MS::kSuccess;
    }

    MStatus status;
    MDagPath transformPath;
    status = MDagPath::getAPathTo(transformObject, transformPath);
    if (!status)
    {
        return MS::kSuccess;
    }

    MFnDagNode transformDagNode(transformObject, &status);
    if (!status)
    {
        return MS::kSuccess;
    }

    MStringArray meshPaths;
    MStringArray historyNodeNames;
    for (unsigned int childIndex = 0; childIndex < transformDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = transformDagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            status = MS::kSuccess;
            continue;
        }

        MDagPath childPath;
        status = MDagPath::getAPathTo(childObject, childPath);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        meshPaths.append(childPath.fullPathName());

        MStringArray historyNames;
        status = maya_cmd::GetPrunedHistory(childPath.fullPathName(), historyNames);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        for (unsigned int historyIndex = 0; historyIndex < historyNames.length(); ++historyIndex)
        {
            MSelectionList historySelection;
            if (historySelection.add(historyNames[historyIndex]) != MS::kSuccess)
            {
                continue;
            }

            MObject historyObject;
            if (historySelection.getDependNode(0, historyObject) != MS::kSuccess || historyObject.isNull())
            {
                continue;
            }

            MFnDependencyNode historyNode(historyObject, &status);
            if (!status)
            {
                status = MS::kSuccess;
                continue;
            }

            if (!ShouldDeleteMeshHistoryNodeType(historyNode.typeName()))
            {
                continue;
            }

            bool alreadyQueued = false;
            for (unsigned int existingIndex = 0; existingIndex < historyNodeNames.length(); ++existingIndex)
            {
                if (historyNodeNames[existingIndex] == historyNames[historyIndex])
                {
                    alreadyQueued = true;
                    break;
                }
            }
            if (!alreadyQueued)
            {
                historyNodeNames.append(historyNames[historyIndex]);
            }
        }
    }

    if (meshPaths.length() == 0)
    {
        return MS::kSuccess;
    }

    for (unsigned int historyIndex = 0; historyIndex < historyNodeNames.length(); ++historyIndex)
    {
        if (!DependencyNodeExists(historyNodeNames[historyIndex]))
        {
            continue;
        }

        status = maya_cmd::DeleteNodeByName(historyNodeNames[historyIndex]);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    for (unsigned int meshIndex = 0; meshIndex < meshPaths.length(); ++meshIndex)
    {
        if (!DependencyNodeExists(meshPaths[meshIndex]))
        {
            continue;
        }

        status = maya_cmd::DeleteNodeByName(meshPaths[meshIndex]);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MObject FindNodeByName(const MString &nodeName)
{
    MSelectionList selection;
    if (selection.add(nodeName) != MS::kSuccess)
    {
        return MObject::kNullObj;
    }

    MObject object;
    if (selection.getDependNode(0, object) != MS::kSuccess)
    {
        return MObject::kNullObj;
    }

    return object;
}

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
}

SmdMeshImporter::SmdMeshImporter(
    std::shared_ptr<const simple_smd::Document> document,
    std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone,
    dcc_import_policy::SceneImportPolicy scenePolicy,
    dcc_import_transform::TransformCorrection transformCorrection)
    : document_(document)
    , jointPathsByBone_(jointPathsByBone)
    , scenePolicy_(std::move(scenePolicy))
    , transformCorrection_(std::move(transformCorrection))
{
}

MStatus SmdMeshImporter::Import(MObject parent) const
{
    if (document_->triangles.empty())
    {
        return MS::kSuccess;
    }

    std::vector<std::string> materialNames;
    materialNames.reserve(document_->triangles.size());
    for (const simple_smd::Triangle &triangle : document_->triangles)
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
    for (const simple_smd::Triangle &triangle : document_->triangles)
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
            normalVertexIds.append(nextVertexIndex);
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
    MObject transformObject = MObject::kNullObj;
    if (dcc_import_policy::UsesExistingObjectMerge(scenePolicy_))
    {
        transformObject = findExistingMeshGroup(parent, materialName);
    }

    const bool reusedExistingGroup = !transformObject.isNull();
    if (!reusedExistingGroup)
    {
        MFnTransform transformFn;
        transformObject = transformFn.create(parent, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create mesh transform for material group ") + materialName.c_str(), status);
        }
        transformFn.setName((smd_mesh_import_impl::SanitizeMeshName(materialName) + "_grp#").c_str());
    }

    if (!dcc_import_policy::UsesAppendMissingObjects(scenePolicy_) && !transformCorrection_.IsIdentity())
    {
        MFnTransform transformFn(transformObject, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access mesh group transform for ") + materialName.c_str(), status);
        }

        status = transformFn.setTranslation(transformCorrection_.translation, MSpace::kTransform);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to set mesh group translation for ") + materialName.c_str(), status);
        }

        status = transformFn.setRotation(transformCorrection_.RotationQuaternion());
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to set mesh group rotation for ") + materialName.c_str(), status);
        }

        status = transformFn.setScale(const_cast<double *>(transformCorrection_.scale));
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to apply import correction to mesh group ") + materialName.c_str(), status);
        }
    }

    const MObject existingMeshObject = reusedExistingGroup ? findPrimaryMeshChild(transformObject) : MObject::kNullObj;
    const bool hasExistingMeshChild = !existingMeshObject.isNull();
    const bool canReuseExistingMeshForUpdate =
        hasExistingMeshChild &&
        dcc_import_policy::UsesUpdateCurrentScene(scenePolicy_) &&
        hasWeights &&
        meshTopologyMatches(existingMeshObject, polygonCounts, polygonConnects);

    if (hasExistingMeshChild)
    {
        if (canReuseExistingMeshForUpdate)
        {
        }
        else if (dcc_import_policy::UsesUpdateCurrentScene(scenePolicy_))
        {
            status = smd_mesh_import_impl::DeleteExistingMeshGroupForUpdate(scenePolicy_, transformObject);
            if (!status)
            {
                return maya_smd::ReportError(MString("maya_smd: failed to clear existing mesh for material group ") + materialName.c_str(), status);
            }
        }
        else
        {
            return MS::kSuccess;
        }
    }

    MFnMesh meshFn;
    MObject meshObject = MObject::kNullObj;
    if (canReuseExistingMeshForUpdate)
    {
        meshObject = existingMeshObject;
        meshFn.setObject(meshObject);
        status = meshFn.setPoints(points, MSpace::kObject);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to update mesh points for material group ") + materialName.c_str(), status);
        }
    }
    else
    {
        status = smd_mesh_import_impl::DeleteExistingMeshGroupForUpdate(scenePolicy_, transformObject);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to clear mesh history for material group ") + materialName.c_str(), status);
        }

        meshObject = meshFn.create(points.length(), polygonCounts.length(), points, polygonCounts, polygonConnects, transformObject, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create mesh shape for material group ") + materialName.c_str(), status);
        }

        meshFn.setName((smd_mesh_import_impl::SanitizeMeshName(materialName) + "Shape#").c_str());
    }

    status = smd_mesh_import_impl::SetStringAttribute(transformObject, smd_mesh_import_impl::kSmdMaterialNameAttribute, materialName);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to tag mesh transform with SMD material name for ") + materialName.c_str(), status);
    }

    status = smd_mesh_import_impl::SetStringAttribute(meshObject, smd_mesh_import_impl::kSmdMaterialNameAttribute, materialName);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to tag mesh shape with SMD material name for ") + materialName.c_str(), status);
    }

    status = meshFn.setUVs(uValues, vValues);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to set UV set for material group ") + materialName.c_str(), status);
    }

    status = meshFn.assignUVs(polygonCounts, uvIds);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to assign UVs for material group ") + materialName.c_str(), status);
    }

    status = meshFn.setFaceVertexNormals(normals, normalFaceIds, normalVertexIds);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to assign normals for material group ") + materialName.c_str(), status);
    }

    MDagPath meshTransformPath;
    status = MDagPath::getAPathTo(transformObject, meshTransformPath);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve transform path for material group ") + materialName.c_str(), status);
    }

    status = assignMaterial(materialName, meshObject);
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

MObject SmdMeshImporter::findExistingMeshGroup(MObject parent, const std::string &materialName) const
{
    const std::string expectedName = smd_mesh_import_impl::SanitizeMeshName(materialName) + "_grp";

    MStatus status;
    if (parent.isNull())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) != MS::kSuccess || !dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint))
            {
                continue;
            }

            MFnDagNode dagNode(dagPath, &status);
            if (status && dcc_import_policy::MatchesNodePrefixForAppend(scenePolicy_, dagNode.name().asChar(), expectedName))
            {
                return dagPath.node();
            }
        }
        return MObject::kNullObj;
    }

    MFnDagNode parentDagNode(parent, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < parentDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = parentDagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint))
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDagNode childDagNode(childObject, &status);
        if (status && dcc_import_policy::MatchesNodePrefixForAppend(scenePolicy_, childDagNode.name().asChar(), expectedName))
        {
            return childObject;
        }
        status = MS::kSuccess;
    }

    return MObject::kNullObj;
}

MStatus SmdMeshImporter::assignMaterial(const std::string &materialName, const MObject &meshObject) const
{
    const std::string sanitizedBaseName = smd_mesh_import_impl::SanitizeMeshName(materialName);
    const MString shaderName = (sanitizedBaseName.empty() ? std::string("smdMaterial") : sanitizedBaseName).c_str();
    const MString shadingGroupName = shaderName + "_SG";

    MStatus status;

    MObject shaderObject = smd_mesh_import_impl::FindNodeByName(shaderName);
    if (shaderObject.isNull())
    {
        status = maya_cmd::CreateNamedDependencyNode("lambert", shaderName, shaderObject);
        if (!status || shaderObject.isNull())
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create lambert shader for material group ") + materialName.c_str(), status);
        }
    }

    MObject shadingGroupObject = smd_mesh_import_impl::FindNodeByName(shadingGroupName);
    if (shadingGroupObject.isNull())
    {
        status = maya_cmd::EnsureRenderableShadingGroup(shadingGroupName, shadingGroupObject);
        if (!status || shadingGroupObject.isNull())
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create shading group for material group ") + materialName.c_str(), status);
        }
    }

    MDagPath meshPath;
    status = MDagPath::getAPathTo(meshObject, meshPath);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve mesh path for material binding ") + materialName.c_str(), status);
    }

    MFnDependencyNode shaderFn(shaderObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind shader node for material group ") + materialName.c_str(), status);
    }

    MFnDependencyNode shadingGroupFn(shadingGroupObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind shading group node for material group ") + materialName.c_str(), status);
    }

    MPlug outColorPlug = shaderFn.findPlug("outColor", true, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve shader outColor plug for material group ") + materialName.c_str(), status);
    }

    MPlug surfaceShaderPlug = shadingGroupFn.findPlug("surfaceShader", true, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve shading group surfaceShader plug for material group ") + materialName.c_str(), status);
    }

    status = maya_cmd::ConnectPlugsForce(outColorPlug, surfaceShaderPlug);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to connect shader to shading group for material group ") + materialName.c_str(), status);
    }

    status = maya_cmd::AddDagPathToSet(meshPath, shadingGroupObject);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to assign mesh to shading group for material group ") + materialName.c_str(), status);
    }

    return MS::kSuccess;
}

MObject SmdMeshImporter::findPrimaryMeshChild(const MObject &transformObject) const
{
    MStatus status;
    MFnDagNode dagNode(transformObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            continue;
        }

        MFnDagNode meshDagNode(childObject, &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            return childObject;
        }
    }

    return MObject::kNullObj;
}

bool SmdMeshImporter::meshTopologyMatches(
    const MObject &meshObject,
    const MIntArray &polygonCounts,
    const MIntArray &polygonConnects) const
{
    MStatus status;
    MFnMesh meshFn(meshObject, &status);
    if (!status)
    {
        return false;
    }

    MIntArray existingCounts;
    MIntArray existingConnects;
    status = meshFn.getVertices(existingCounts, existingConnects);
    if (!status)
    {
        return false;
    }

    if (existingCounts.length() != polygonCounts.length() || existingConnects.length() != polygonConnects.length())
    {
        return false;
    }

    for (unsigned int index = 0; index < polygonCounts.length(); ++index)
    {
        if (existingCounts[index] != polygonCounts[index])
        {
            return false;
        }
    }

    for (unsigned int index = 0; index < polygonConnects.length(); ++index)
    {
        if (existingConnects[index] != polygonConnects[index])
        {
            return false;
        }
    }

    return true;
}

MObject SmdMeshImporter::findExistingSkinClusterNode(const MObject &meshObject) const
{
    if (!dcc_import_policy::UsesUpdateCurrentScene(scenePolicy_) || meshObject.isNull())
    {
        return MObject::kNullObj;
    }

    MObject rootObject = meshObject;
    MStatus status;
    MItDependencyGraph iterator(
        rootObject,
        MFn::kSkinClusterFilter,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        MObject currentNode = iterator.currentItem(&status);
        if (!status || currentNode.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        return currentNode;
    }

    return MObject::kNullObj;
}

MStatus SmdMeshImporter::updateExistingSkinClusterBindings(
    const MObject &skinClusterObject,
    const MDagPathArray &influencePaths,
    const MDagPath &meshParentPath) const
{
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

    MDagPathArray existingInfluencePaths;
    status = smd_mesh_import_impl::EnsureSkinClusterContainsInfluences(scenePolicy_, skinClusterObject, influencePaths, existingInfluencePaths);
    if (!status)
    {
        return MS::kFailure;
    }

    std::unordered_map<std::string, unsigned int> existingInfluenceByPath =
        smd_mesh_import_impl::BuildInfluenceIndexByPath(existingInfluencePaths);

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug bindPreMatrixArrayPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        const std::string fullPath = influencePaths[influenceIndex].fullPathName().asChar();
        const auto existingIt = existingInfluenceByPath.find(fullPath);
        if (existingIt == existingInfluenceByPath.end())
        {
            return maya_smd::ReportWarning(
                MString("maya_smd: update skipped skinCluster overwrite because an existing influence did not match for ")
                + meshParentPath.fullPathName());
        }

        MDagPath matchedInfluencePath;
        if (!smd_mesh_import_impl::FindMatchingInfluencePath(scenePolicy_, existingInfluencePaths, influencePaths[influenceIndex], matchedInfluencePath))
        {
            return maya_smd::ReportWarning(
                MString("maya_smd: update skipped skinCluster overwrite because a required influence path could not be resolved for ")
                + meshParentPath.fullPathName());
        }

        const unsigned int logicalInfluenceIndex = skinClusterFn.indexForInfluenceObject(matchedInfluencePath, &status);
        if (!status)
        {
            return MS::kFailure;
        }

        MPlug bindPreMatrixPlug = bindPreMatrixArrayPlug.elementByLogicalIndex(logicalInfluenceIndex, &status);
        if (!status)
        {
            return MS::kFailure;
        }

        MFnMatrixData matrixDataFn;
        MObject bindPreMatrixObject = matrixDataFn.create(influencePaths[influenceIndex].inclusiveMatrixInverse(), &status);
        if (!status)
        {
            return MS::kFailure;
        }
        status = bindPreMatrixPlug.setMObject(bindPreMatrixObject);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnMatrixData geomMatrixDataFn;
    MObject geomMatrixObject = geomMatrixDataFn.create(meshParentPath.inclusiveMatrix(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    status = geomMatrixPlug.setMObject(geomMatrixObject);
    return status ? MS::kSuccess : MS::kFailure;
}

MStatus SmdMeshImporter::createSkinClusterWithApi(
    const MDagPathArray &influencePaths,
    const MDagPath &meshDagPath,
    const MDagPath &meshParentPath,
    MObject &skinClusterObject) const
{
    skinClusterObject = MObject::kNullObj;

    MStatus status;
    MFnMesh meshFn(meshDagPath, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind MFnMesh for skinning ") + meshDagPath.fullPathName(), status);
    }

    const MString originalShapeName = meshFn.name() + "Orig";
    MObject originalMeshObject = meshFn.copy(meshDagPath.node(), meshParentPath.node(), &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to duplicate original mesh for skinning ") + meshDagPath.fullPathName(), status);
    }

    MFnDependencyNode originalMeshNode(originalMeshObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to access duplicated original mesh for skinning ") + meshDagPath.fullPathName(), status);
    }
    originalMeshNode.setName(originalShapeName);

    MPlug intermediatePlug = originalMeshNode.findPlug("intermediateObject", true, &status);
    if (status)
    {
        intermediatePlug.setBool(true);
    }
    status = MS::kSuccess;

    MFnDependencyNode skinClusterNodeFn;
    skinClusterObject = skinClusterNodeFn.create("skinCluster", "mayaSmdSkinCluster#", &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to create skinCluster node for ") + meshDagPath.fullPathName(), status);
    }

    MDGModifier dgModifier;
    auto connectArrayPlug = [&](const MObject &srcNode, const char *srcAttr, unsigned int srcIndex,
                                const MObject &dstNode, const char *dstAttr, unsigned int dstIndex) -> MStatus
    {
        MFnDependencyNode srcFn(srcNode);
        MFnDependencyNode dstFn(dstNode);
        MPlug srcPlug = srcFn.findPlug(srcAttr, true, &status);
        if (!status)
        {
            return MS::kFailure;
        }
        MPlug dstPlug = dstFn.findPlug(dstAttr, true, &status);
        if (!status)
        {
            return MS::kFailure;
        }
        if (srcPlug.isArray())
        {
            srcPlug = srcPlug.elementByLogicalIndex(srcIndex, &status);
            if (!status)
            {
                return MS::kFailure;
            }
        }
        if (dstPlug.isArray())
        {
            dstPlug = dstPlug.elementByLogicalIndex(dstIndex, &status);
            if (!status)
            {
                return MS::kFailure;
            }
        }
        return dgModifier.connect(srcPlug, dstPlug);
    };

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind dependency node for skinCluster on ") + meshDagPath.fullPathName(), status);
    }

    MPlug inputPlug = skinClusterNode.findPlug("input", true, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to find skinCluster input plug.", status);
    }
    inputPlug = inputPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to resolve skinCluster input[0].", status);
    }
    MPlug inputGeometryPlug = inputPlug.child(0, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to resolve skinCluster inputGeometry plug.", status);
    }

    MPlug sourceWorldMeshPlug = originalMeshNode.findPlug("worldMesh", true, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to find original mesh worldMesh plug.", status);
    }
    sourceWorldMeshPlug = sourceWorldMeshPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to resolve original mesh worldMesh[0].", status);
    }
    status = dgModifier.connect(sourceWorldMeshPlug, inputGeometryPlug);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to connect original mesh to skinCluster inputGeometry.", status);
    }

    status = connectArrayPlug(originalMeshObject, "outMesh", 0, skinClusterObject, "originalGeometry", 0);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to connect originalGeometry for skinCluster.", status);
    }
    status = connectArrayPlug(skinClusterObject, "outputGeometry", 0, meshDagPath.node(), "inMesh", 0);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to connect skinCluster outputGeometry to mesh.", status);
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        status = connectArrayPlug(influencePaths[influenceIndex].node(), "worldMatrix", 0, skinClusterObject, "matrix", influenceIndex);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to connect influence worldMatrix for ") + influencePaths[influenceIndex].fullPathName(), status);
        }

        MPlug bindPreMatrixPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find bindPreMatrix plug.", status);
        }
        bindPreMatrixPlug = bindPreMatrixPlug.elementByLogicalIndex(influenceIndex, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to resolve bindPreMatrix element.", status);
        }

        MFnMatrixData matrixDataFn;
        MObject bindPreMatrixObject = matrixDataFn.create(influencePaths[influenceIndex].inclusiveMatrixInverse(), &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to create bindPreMatrix value.", status);
        }
        status = bindPreMatrixPlug.setMObject(bindPreMatrixObject);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to assign bindPreMatrix value.", status);
        }
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to find geomMatrix plug.", status);
    }
    MFnMatrixData geomMatrixDataFn;
    MObject geomMatrixObject = geomMatrixDataFn.create(meshParentPath.inclusiveMatrix(), &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to create geomMatrix value.", status);
    }
    status = geomMatrixPlug.setMObject(geomMatrixObject);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to assign geomMatrix value.", status);
    }

    status = dgModifier.doIt();
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to finalize skinCluster graph for ") + meshDagPath.fullPathName(), status);
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

            if (jointPathsByBone_->find(weight.boneIndex) == jointPathsByBone_->end())
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

    MDagPathArray activeInfluencePaths;
    for (int boneIndex : activeBoneIndices)
    {
        const auto jointIt = jointPathsByBone_->find(boneIndex);
        if (jointIt == jointPathsByBone_->end())
        {
            continue;
        }
        activeInfluencePaths.append(jointIt->second);
    }

    if (activeInfluencePaths.length() == 0)
    {
        return MS::kSuccess;
    }

    MStatus status;
    MObject meshObject = findPrimaryMeshChild(meshTransformPath.node());
    if (meshObject.isNull())
    {
        return maya_smd::ReportError(MString("maya_smd: failed to find mesh shape for skinning ") + meshTransformPath.fullPathName());
    }

    MDagPath meshDagPath;
    status = MDagPath::getAPathTo(meshObject, meshDagPath);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve mesh DAG path for skinning ") + meshTransformPath.fullPathName(), status);
    }

    MObject skinClusterObject = findExistingSkinClusterNode(meshObject);
    if (!skinClusterObject.isNull())
    {
        status = updateExistingSkinClusterBindings(skinClusterObject, activeInfluencePaths, meshTransformPath);
        if (!status)
        {
            return status;
        }
    }
    else
    {
        status = createSkinClusterWithApi(activeInfluencePaths, meshDagPath, meshTransformPath, skinClusterObject);
        if (!status || skinClusterObject.isNull())
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create skinCluster for ") + meshTransformPath.fullPathName(), status);
        }
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind MFnSkinCluster for ") + meshTransformPath.fullPathName(), status);
    }

    MDagPathArray influencePaths;
    skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to query skin influences for ") + meshTransformPath.fullPathName(), status);
    }

    MIntArray influenceIndices;
    std::unordered_map<int, unsigned int> boneToInfluenceSlot;
    for (int boneIndex : activeBoneIndices)
    {
        const auto jointIt = jointPathsByBone_->find(boneIndex);
        if (jointIt == jointPathsByBone_->end())
        {
            continue;
        }

        MDagPath matchedInfluencePath;
        if (!smd_mesh_import_impl::FindMatchingInfluencePath(scenePolicy_, influencePaths, jointIt->second, matchedInfluencePath))
        {
            return maya_smd::ReportError(
                MString("maya_smd: failed to match skin influence for bone index ") + std::to_string(boneIndex).c_str(),
                MS::kFailure);
        }

        for (unsigned int influencePathIndex = 0; influencePathIndex < influencePaths.length(); ++influencePathIndex)
        {
            if (matchedInfluencePath.fullPathName() == influencePaths[influencePathIndex].fullPathName())
            {
                boneToInfluenceSlot[boneIndex] = influencePathIndex;
                break;
            }
        }

        if (boneToInfluenceSlot.find(boneIndex) == boneToInfluenceSlot.end())
        {
            return maya_smd::ReportError(
                MString("maya_smd: failed to resolve matched skin influence slot for bone index ") + std::to_string(boneIndex).c_str(),
                MS::kFailure);
        }
    }

    for (unsigned int influencePathIndex = 0; influencePathIndex < influencePaths.length(); ++influencePathIndex)
    {
        influenceIndices.append(static_cast<int>(influencePathIndex));
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to bind dependency node for skinCluster on ") + meshTransformPath.fullPathName(), status);
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
    status = MS::kSuccess;

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to create vertex component for skinning ") + meshTransformPath.fullPathName(), status);
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexLinks.size(); ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to populate vertex component for skinning ") + meshTransformPath.fullPathName(), status);
    }

    MFloatArray weights;
    weights.setLength(static_cast<unsigned int>(vertexLinks.size()) * influenceIndices.length());
    for (unsigned int weightIndex = 0; weightIndex < weights.length(); ++weightIndex)
    {
        weights[weightIndex] = 0.0f;
    }

    unsigned int maxAssignedInfluenceCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexLinks.size(); ++vertexIndex)
    {
        float totalWeight = 0.0f;
        unsigned int assignedInfluenceCount = 0;
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

        for (unsigned int influenceSlot = 0; influenceSlot < influenceIndices.length(); ++influenceSlot)
        {
            if (weights[vertexIndex * influenceIndices.length() + influenceSlot] > 1.0e-6f)
            {
                ++assignedInfluenceCount;
            }
        }
        maxAssignedInfluenceCount = std::max(maxAssignedInfluenceCount, assignedInfluenceCount);

        if (totalWeight > 1.0e-6f)
        {
            const float invTotalWeight = 1.0f / totalWeight;
            for (unsigned int influenceSlot = 0; influenceSlot < influenceIndices.length(); ++influenceSlot)
            {
                weights[vertexIndex * influenceIndices.length() + influenceSlot] *= invTotalWeight;
            }
        }
    }

    if (!maxInfluencesPlug.isNull())
    {
        maxInfluencesPlug.setInt(static_cast<int>(std::max(1u, maxAssignedInfluenceCount)));
    }

    status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to apply skin weights to ") + meshTransformPath.fullPathName(), status);
    }

    return MS::kSuccess;
}
