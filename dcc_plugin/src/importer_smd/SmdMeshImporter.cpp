#include "SmdMeshImporter.h"

#include <common/MayaCommandUtils.h>
#include <common/TransformCorrection.h>
#include <common/MaterialUtils.h>
#include <common/SkinClusterUtils.h>
#include <common_smd/MayaSmdCommon.h>

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>

#include <maya/MDagPathArray.h>
#include <maya/MDGModifier.h>
#include <maya/MFloatArray.h>
#include <maya/MFnAttribute.h>
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
constexpr const char *kSmdRawVertexMapAttribute = "mayaSmdRawVertexMap";
constexpr double kPositionEpsilon = 1.0e-6;
constexpr double kUvEpsilon = 1.0e-6;
constexpr double kWeightEpsilon = 1.0e-6;
constexpr double kNormalContinuityDotThreshold = 0.999;

struct QuantizedPointKey
{
    int64_t x = 0;
    int64_t y = 0;
    int64_t z = 0;

    bool operator==(const QuantizedPointKey &other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct QuantizedUvKey
{
    unsigned int vertexIndex = 0;
    int64_t u = 0;
    int64_t v = 0;

    bool operator==(const QuantizedUvKey &other) const
    {
        return vertexIndex == other.vertexIndex && u == other.u && v == other.v;
    }
};

struct InfluenceSignature
{
    std::vector<std::pair<int, double>> entries;

    bool operator==(const InfluenceSignature &other) const
    {
        if (entries.size() != other.entries.size())
        {
            return false;
        }

        for (size_t index = 0; index < entries.size(); ++index)
        {
            if (entries[index].first != other.entries[index].first ||
                std::abs(entries[index].second - other.entries[index].second) > kWeightEpsilon)
            {
                return false;
            }
        }

        return true;
    }
};

struct SharedVertexCandidate
{
    unsigned int vertexIndex = 0;
    MPoint point;
    MVector normal;
    InfluenceSignature influenceSignature;
};

struct QuantizedPointKeyHasher
{
    size_t operator()(const QuantizedPointKey &key) const
    {
        const size_t hx = std::hash<int64_t>()(key.x);
        const size_t hy = std::hash<int64_t>()(key.y);
        const size_t hz = std::hash<int64_t>()(key.z);
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

struct QuantizedUvKeyHasher
{
    size_t operator()(const QuantizedUvKey &key) const
    {
        const size_t hv = std::hash<unsigned int>()(key.vertexIndex);
        const size_t hu = std::hash<int64_t>()(key.u);
        const size_t hvv = std::hash<int64_t>()(key.v);
        return hv ^ (hu << 1) ^ (hvv << 2);
    }
};

int64_t QuantizeValue(double value, double epsilon)
{
    return static_cast<int64_t>(std::llround(value / epsilon));
}

QuantizedPointKey BuildPointKey(const simple_smd::TriangleVertex &vertex)
{
    QuantizedPointKey key;
    key.x = QuantizeValue(vertex.px, kPositionEpsilon);
    key.y = QuantizeValue(vertex.py, kPositionEpsilon);
    key.z = QuantizeValue(vertex.pz, kPositionEpsilon);
    return key;
}

QuantizedUvKey BuildUvKey(unsigned int vertexIndex, const simple_smd::TriangleVertex &vertex)
{
    QuantizedUvKey key;
    key.vertexIndex = vertexIndex;
    key.u = QuantizeValue(vertex.u, kUvEpsilon);
    key.v = QuantizeValue(vertex.v, kUvEpsilon);
    return key;
}

std::vector<simple_smd::TriangleWeight> BuildNormalizedWeights(const simple_smd::TriangleVertex &vertex)
{
    std::vector<simple_smd::TriangleWeight> normalizedWeights = vertex.links;
    std::sort(normalizedWeights.begin(), normalizedWeights.end(), [](const simple_smd::TriangleWeight &left, const simple_smd::TriangleWeight &right) {
        return left.boneIndex < right.boneIndex;
    });

    std::vector<simple_smd::TriangleWeight> mergedWeights;
    for (const simple_smd::TriangleWeight &weight : normalizedWeights)
    {
        if (weight.weight <= kWeightEpsilon)
        {
            continue;
        }

        if (!mergedWeights.empty() && mergedWeights.back().boneIndex == weight.boneIndex)
        {
            mergedWeights.back().weight += weight.weight;
            continue;
        }

        mergedWeights.push_back(weight);
    }

    if (!mergedWeights.empty())
    {
        return mergedWeights;
    }

    if (vertex.parentBoneIndex >= 0)
    {
        simple_smd::TriangleWeight rigidWeight;
        rigidWeight.boneIndex = vertex.parentBoneIndex;
        rigidWeight.weight = 1.0;
        mergedWeights.push_back(rigidWeight);
    }

    return mergedWeights;
}

InfluenceSignature BuildInfluenceSignature(const simple_smd::TriangleVertex &vertex)
{
    InfluenceSignature signature;
    const std::vector<simple_smd::TriangleWeight> normalizedWeights = BuildNormalizedWeights(vertex);
    signature.entries.reserve(normalizedWeights.size());
    for (const simple_smd::TriangleWeight &weight : normalizedWeights)
    {
        signature.entries.emplace_back(weight.boneIndex, weight.weight);
    }
    return signature;
}

bool PointsMatch(const MPoint &left, const simple_smd::TriangleVertex &right)
{
    return std::abs(left.x - right.px) <= kPositionEpsilon &&
        std::abs(left.y - right.py) <= kPositionEpsilon &&
        std::abs(left.z - right.pz) <= kPositionEpsilon;
}

bool NormalsAreContinuous(const MVector &left, const simple_smd::TriangleVertex &right)
{
    MVector rightNormal(right.nx, right.ny, right.nz);
    const double leftLength = left.length();
    const double rightLength = rightNormal.length();
    if (leftLength <= 1.0e-8 || rightLength <= 1.0e-8)
    {
        return (left - rightNormal).length() <= 1.0e-4;
    }

    const double dot = (left / leftLength) * (rightNormal / rightLength);
    return dot >= kNormalContinuityDotThreshold;
}

unsigned int ResolveSharedVertexIndex(
    const simple_smd::TriangleVertex &vertex,
    const std::vector<simple_smd::TriangleWeight> &normalizedWeights,
    const InfluenceSignature &influenceSignature,
    MPointArray &points,
    std::vector<std::vector<simple_smd::TriangleWeight>> &vertexLinks,
    std::unordered_map<QuantizedPointKey, std::vector<SharedVertexCandidate>, QuantizedPointKeyHasher> &candidatesByPoint)
{
    const QuantizedPointKey pointKey = BuildPointKey(vertex);
    std::vector<SharedVertexCandidate> &candidates = candidatesByPoint[pointKey];
    for (const SharedVertexCandidate &candidate : candidates)
    {
        if (!(candidate.influenceSignature == influenceSignature) ||
            !PointsMatch(candidate.point, vertex) ||
            !NormalsAreContinuous(candidate.normal, vertex))
        {
            continue;
        }

        return candidate.vertexIndex;
    }

    const unsigned int newVertexIndex = points.length();
    points.append(vertex.px, vertex.py, vertex.pz);
    vertexLinks.push_back(normalizedWeights);

    SharedVertexCandidate candidate;
    candidate.vertexIndex = newVertexIndex;
    candidate.point = MPoint(vertex.px, vertex.py, vertex.pz);
    candidate.normal = MVector(vertex.nx, vertex.ny, vertex.nz);
    candidate.influenceSignature = influenceSignature;
    candidates.push_back(candidate);
    return newVertexIndex;
}

unsigned int ResolveUvIndex(
    unsigned int sharedVertexIndex,
    const simple_smd::TriangleVertex &vertex,
    bool flipUvV,
    MFloatArray &uValues,
    MFloatArray &vValues,
    std::unordered_map<QuantizedUvKey, unsigned int, QuantizedUvKeyHasher> &uvIndexByKey)
{
    const QuantizedUvKey uvKey = BuildUvKey(sharedVertexIndex, vertex);
    auto existingIt = uvIndexByKey.find(uvKey);
    if (existingIt != uvIndexByKey.end())
    {
        return existingIt->second;
    }

    const unsigned int uvIndex = uValues.length();
    uValues.append(static_cast<float>(vertex.u));
    vValues.append(static_cast<float>(flipUvV ? (1.0 - vertex.v) : vertex.v));
    uvIndexByKey[uvKey] = uvIndex;
    return uvIndex;
}

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

MStatus SetStringAttribute(MObject nodeObject, const char *attributeName, const std::string &value, bool hidden = false)
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
        attributeFn.setHidden(hidden);

        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    MFnAttribute attributeFn(attributeObject, &status);
    if (status)
    {
        attributeFn.setKeyable(false);
        attributeFn.setHidden(hidden);
    }
    status = MS::kSuccess;

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

}

std::string SerializeRawVertexMap(const std::vector<std::pair<int, unsigned int>> &rawToSharedVertexIndex)
{
    std::ostringstream stream;
    for (const auto &entry : rawToSharedVertexIndex)
    {
        stream << entry.first << ' ' << entry.second << '\n';
    }
    return stream.str();
}

SmdMeshImporter::SmdMeshImporter(
    std::shared_ptr<const simple_smd::Document> document,
    std::shared_ptr<const std::unordered_map<int, MDagPath>> jointPathsByBone,
    dcc_import_policy::SceneImportPolicy scenePolicy,
    dcc_import_transform::TransformCorrection transformCorrection,
    bool flipUvV)
    : document_(document)
    , jointPathsByBone_(jointPathsByBone)
    , scenePolicy_(std::move(scenePolicy))
    , transformCorrection_(std::move(transformCorrection))
    , flipUvV_(flipUvV)
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
    std::unordered_map<
        smd_mesh_import_impl::QuantizedPointKey,
        std::vector<smd_mesh_import_impl::SharedVertexCandidate>,
        smd_mesh_import_impl::QuantizedPointKeyHasher> candidatesByPoint;
    std::unordered_map<
        smd_mesh_import_impl::QuantizedUvKey,
        unsigned int,
        smd_mesh_import_impl::QuantizedUvKeyHasher> uvIndexByKey;
    std::vector<std::pair<int, unsigned int>> rawToSharedVertexIndex;

    bool hasWeights = false;
    int faceIndex = 0;
    int globalRawVertexIndex = 0;
    for (const simple_smd::Triangle &triangle : document_->triangles)
    {
        const bool matchesMaterial = triangle.materialName == materialName && triangle.vertices.size() == 3;
        if (!matchesMaterial)
        {
            globalRawVertexIndex += static_cast<int>(triangle.vertices.size());
            continue;
        }

        polygonCounts.append(3);
        for (int vertexInFace = 0; vertexInFace < 3; ++vertexInFace)
        {
            const simple_smd::TriangleVertex &vertex = triangle.vertices[vertexInFace];
            const std::vector<simple_smd::TriangleWeight> normalizedWeights =
                smd_mesh_import_impl::BuildNormalizedWeights(vertex);
            const smd_mesh_import_impl::InfluenceSignature influenceSignature =
                smd_mesh_import_impl::BuildInfluenceSignature(vertex);
            const unsigned int sharedVertexIndex = smd_mesh_import_impl::ResolveSharedVertexIndex(
                vertex,
                normalizedWeights,
                influenceSignature,
                points,
                vertexLinks,
                candidatesByPoint);
            rawToSharedVertexIndex.emplace_back(globalRawVertexIndex, sharedVertexIndex);
            polygonConnects.append(static_cast<int>(sharedVertexIndex));
            uvIds.append(static_cast<int>(smd_mesh_import_impl::ResolveUvIndex(
                sharedVertexIndex,
                vertex,
                flipUvV_,
                uValues,
                vValues,
                uvIndexByKey)));
            normals.append(MVector(vertex.nx, vertex.ny, vertex.nz));
            normalFaceIds.append(faceIndex);
            normalVertexIds.append(static_cast<int>(sharedVertexIndex));
            hasWeights = hasWeights || !normalizedWeights.empty();
            ++globalRawVertexIndex;
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

    status = smd_mesh_import_impl::SetStringAttribute(
        meshObject,
        smd_mesh_import_impl::kSmdRawVertexMapAttribute,
        SerializeRawVertexMap(rawToSharedVertexIndex),
        true);
    if (!status)
    {
        return maya_smd::ReportError(
            MString("maya_smd: failed to store raw vertex map for material group ")
            + materialName.c_str(),
            status);
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
    MStatus status;
    const dcc_material::MaterialNodeNames materialNames =
        dcc_material::BuildMaterialNodeNames(materialName, "smdMaterial");

    MObject shaderObject;
    MObject shadingGroupObject;
    status = dcc_material::EnsureSurfaceShaderBinding(
        "lambert",
        materialNames.shaderName,
        materialNames.shadingGroupName,
        shaderObject,
        shadingGroupObject);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to ensure shader binding for material group ") + materialName.c_str(), status);
    }

    status = dcc_material::AssignWholeMeshToShadingGroup(meshObject, shadingGroupObject);
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
    status = dcc_skinning::EnsureSkinClusterContainsInfluences(scenePolicy_, skinClusterObject, influencePaths, existingInfluencePaths);
    if (!status)
    {
        return MS::kFailure;
    }

    std::unordered_map<std::string, unsigned int> existingInfluenceByPath =
        dcc_skinning::BuildInfluenceIndexByPath(existingInfluencePaths);

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
        if (!dcc_skinning::FindMatchingInfluencePath(scenePolicy_, existingInfluencePaths, influencePaths[influenceIndex], matchedInfluencePath))
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

        status = dcc_skinning::SetSkinClusterBindPreMatrix(
            skinClusterObject,
            logicalInfluenceIndex,
            influencePaths[influenceIndex].inclusiveMatrixInverse());
        if (!status)
        {
            return MS::kFailure;
        }
    }

    status = dcc_skinning::SetSkinClusterGeomMatrix(skinClusterObject, meshParentPath.inclusiveMatrix());
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

        status = dcc_skinning::SetSkinClusterBindPreMatrix(
            skinClusterObject,
            influenceIndex,
            influencePaths[influenceIndex].inclusiveMatrixInverse());
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to assign bindPreMatrix value.", status);
        }
    }

    status = dcc_skinning::SetSkinClusterGeomMatrix(skinClusterObject, meshParentPath.inclusiveMatrix());
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
        if (!dcc_skinning::FindMatchingInfluencePath(scenePolicy_, influencePaths, jointIt->second, matchedInfluencePath))
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

    status = dcc_skinning::BuildPhysicalInfluenceIndices(influencePaths, influenceIndices);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to build physical influence list for ") + meshTransformPath.fullPathName(), status);
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MObject vertexComponent;
    status = dcc_skinning::CreateMeshVertexComponent(static_cast<unsigned int>(vertexLinks.size()), vertexComponent);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to create vertex component for skinning ") + meshTransformPath.fullPathName(), status);
    }

    MFloatArray weights;
    weights.setLength(static_cast<unsigned int>(vertexLinks.size()) * influenceIndices.length());
    for (unsigned int weightIndex = 0; weightIndex < weights.length(); ++weightIndex)
    {
        weights[weightIndex] = 0.0f;
    }

    for (unsigned int vertexIndex = 0; vertexIndex < vertexLinks.size(); ++vertexIndex)
    {
        for (const simple_smd::TriangleWeight &weight : vertexLinks[vertexIndex])
        {
            const auto influenceSlotIt = boneToInfluenceSlot.find(weight.boneIndex);
            if (influenceSlotIt == boneToInfluenceSlot.end() || weight.weight <= 0.0)
            {
                continue;
            }

            const unsigned int influenceSlot = influenceSlotIt->second;
            weights[vertexIndex * influenceIndices.length() + influenceSlot] += static_cast<float>(weight.weight);
        }
    }

    const unsigned int maxAssignedInfluenceCount = dcc_skinning::NormalizeWeightBufferInPlace(
        weights,
        static_cast<unsigned int>(vertexLinks.size()),
        static_cast<unsigned int>(influenceIndices.length()));

    status = dcc_skinning::PrepareSkinClusterForSetWeights(skinClusterObject, maxAssignedInfluenceCount);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to prepare skinCluster weight write state for ") + meshTransformPath.fullPathName(), status);
    }

    status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to apply skin weights to ") + meshTransformPath.fullPathName(), status);
    }

    return MS::kSuccess;
}
