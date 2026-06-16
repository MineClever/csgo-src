#include "ObjSceneImporter.h"

#include <common/MaterialUtils.h>
#include <common_obj/MayaObjCommon.h>

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

namespace obj_scene_importer_detail
{

using VertexKey = std::tuple<int, int, int>;

struct UnifiedMeshData
{
    MFloatPointArray vertexArray;
    MIntArray polygonCounts;
    MIntArray polygonConnects;

    MFloatArray uArray;
    MFloatArray vArray;
    MIntArray uvIds;

    MVectorArray normalArray;
    MIntArray normalFaceList;
    MIntArray normalVertexList;
};

void BuildUnifiedMeshData(
    const rapidobj::Attributes &attributes,
    const rapidobj::Mesh &mesh,
    bool flipUvV,
    UnifiedMeshData &outData)
{
    const auto &positions = attributes.positions;
    const auto &texcoords = attributes.texcoords;
    const auto &normals = attributes.normals;

    const bool hasTexcoords = (texcoords.size() > 0);
    const bool hasNormals = (normals.size() > 0);

    std::map<VertexKey, int> vertexMap;

    const int numFaces = static_cast<int>(mesh.num_face_vertices.size());
    int mayaVertexIndex = 0;
    int idxOffset = 0;

    for (int faceIdx = 0; faceIdx < numFaces; ++faceIdx)
    {
        const int numFaceVerts = static_cast<int>(mesh.num_face_vertices[faceIdx]);
        outData.polygonCounts.append(numFaceVerts);

        for (int v = 0; v < numFaceVerts; ++v)
        {
            const rapidobj::Index &idx = mesh.indices[idxOffset + v];

            const VertexKey key(idx.position_index, idx.texcoord_index, idx.normal_index);

            int currentVertexIdx = -1;
            const auto it = vertexMap.find(key);
            if (it != vertexMap.end())
            {
                currentVertexIdx = it->second;
            }
            else
            {
                currentVertexIdx = mayaVertexIndex++;
                vertexMap[key] = currentVertexIdx;

                const int posBase = 3 * idx.position_index;
                outData.vertexArray.append(MFloatPoint(
                    static_cast<float>(positions[posBase + 0]),
                    static_cast<float>(positions[posBase + 1]),
                    static_cast<float>(positions[posBase + 2])));

                if (hasTexcoords && idx.texcoord_index >= 0)
                {
                    const int uvBase = 2 * idx.texcoord_index;
                    float u = static_cast<float>(texcoords[uvBase + 0]);
                    float vVal = static_cast<float>(texcoords[uvBase + 1]);
                    if (flipUvV)
                    {
                        vVal = 1.0f - vVal;
                    }
                    outData.uArray.append(u);
                    outData.vArray.append(vVal);
                }
                else
                {
                    outData.uArray.append(0.0f);
                    outData.vArray.append(0.0f);
                }
            }

            outData.polygonConnects.append(currentVertexIdx);
            outData.uvIds.append(currentVertexIdx);

            if (hasNormals && idx.normal_index >= 0)
            {
                const int nrmBase = 3 * idx.normal_index;
                outData.normalArray.append(MVector(
                    static_cast<double>(normals[nrmBase + 0]),
                    static_cast<double>(normals[nrmBase + 1]),
                    static_cast<double>(normals[nrmBase + 2])));
                outData.normalFaceList.append(faceIdx);
                outData.normalVertexList.append(v);
            }
        }

        idxOffset += numFaceVerts;
    }
}

} // namespace obj_scene_importer_detail

using namespace obj_scene_importer_detail;

ObjSceneImporter::ObjSceneImporter(
    std::shared_ptr<const simple_obj::Document> document,
    const ObjImportOptions &importOptions)
    : document_(std::move(document))
    , importOptions_(importOptions)
{
}

MStatus ObjSceneImporter::Import()
{
    const MStatus rootStatus = createImportRoot();
    if (!rootStatus)
    {
        return rootStatus;
    }

    const rapidobj::Result &result = document_->GetResult();

    for (const rapidobj::Shape &shape : result.shapes)
    {
        if (shape.mesh.indices.size() == 0)
        {
            continue;
        }

        const MStatus shapeStatus = importShape(shape);
        if (!shapeStatus)
        {
            return shapeStatus;
        }
    }

    return MS::kSuccess;
}

MStatus ObjSceneImporter::createImportRoot()
{
    // useSceneRoot=1: place content directly in the scene, skip wrapper root
    // useSceneRoot=0: create a wrapper root node
    if (importOptions_.useSceneRoot)
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform rootTransformFn;
    importRoot_ = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return maya_obj::ReportError("maya_obj: failed to create OBJ import root.", status);
    }

    rootTransformFn.setName("obj_import_root#");

    return MS::kSuccess;
}

MStatus ObjSceneImporter::importShape(const rapidobj::Shape &shape)
{
    const MObject parent = importRoot_.isNull() ? MObject::kNullObj : importRoot_;

    MObject meshTransformObj;
    MStatus status = createMayaMesh(
        document_->GetResult().attributes,
        shape.mesh,
        shape.name,
        parent,
        meshTransformObj);

    if (!status)
    {
        return status;
    }

    const bool hasMaterialIds = std::any_of(
        shape.mesh.material_ids.begin(),
        shape.mesh.material_ids.end(),
        [](int id) { return id >= 0; });

    if (hasMaterialIds && !document_->GetResult().materials.empty())
    {
        status = assignPerFaceMaterials(
            shape.mesh,
            document_->GetResult().materials,
            meshTransformObj);
        if (!status)
        {
            return status;
        }
    }
    else
    {
        status = assignDefaultMaterial(meshTransformObj);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

MStatus ObjSceneImporter::createMayaMesh(
    const rapidobj::Attributes &attributes,
    const rapidobj::Mesh &mesh,
    const std::string &shapeName,
    MObject parent,
    MObject &outTransformObj)
{
    UnifiedMeshData meshData;
    BuildUnifiedMeshData(attributes, mesh, importOptions_.flipUvV, meshData);

    if (meshData.vertexArray.length() == 0 || meshData.polygonCounts.length() == 0)
    {
        return maya_obj::ReportError("maya_obj: mesh has no vertices or faces");
    }

    MString nodeName = sanitizeNodeName(shapeName);

    MStatus status;
    MFnTransform transformFn;
    MObject transformObj = transformFn.create(parent, &status);
    if (!status)
    {
        return maya_obj::ReportError("maya_obj: failed to create mesh transform", status);
    }

    transformFn.setName(nodeName + "_grp#");

    MFnMesh meshFn;
    MObject meshObj = meshFn.create(
        static_cast<int>(meshData.vertexArray.length()),
        static_cast<int>(meshData.polygonCounts.length()),
        meshData.vertexArray,
        meshData.polygonCounts,
        meshData.polygonConnects,
        transformObj,
        &status);

    if (!status || meshObj.isNull())
    {
        return maya_obj::ReportError("maya_obj: MFnMesh::create() failed", status);
    }

    meshFn.setName(nodeName + "Shape#");

    if (meshData.uArray.length() > 0)
    {
        MString uvSetName("map1");
        MStatus uvStatus = meshFn.setUVs(meshData.uArray, meshData.vArray, &uvSetName);
        if (uvStatus)
        {
            meshFn.assignUVs(meshData.polygonCounts, meshData.uvIds, &uvSetName);
        }
    }

    if (meshData.normalArray.length() > 0)
    {
        meshFn.setFaceVertexNormals(
            meshData.normalArray,
            meshData.normalFaceList,
            meshData.normalVertexList);
    }

    outTransformObj = transformObj;
    return MS::kSuccess;
}

MStatus ObjSceneImporter::assignDefaultMaterial(const MObject &meshTransformObj)
{
    const dcc_material::MaterialNodeNames materialNames =
        dcc_material::BuildMaterialNodeNames("obj_default", "objMaterial");

    MObject shaderObject;
    MObject shadingGroupObject;
    MStatus status = dcc_material::EnsureSurfaceShaderBinding(
        "lambert",
        materialNames.shaderName,
        materialNames.shadingGroupName,
        shaderObject,
        shadingGroupObject);

    if (!status)
    {
        return maya_obj::ReportError("maya_obj: failed to ensure default shader binding", status);
    }

    status = dcc_material::AssignWholeMeshToShadingGroup(meshTransformObj, shadingGroupObject);

    if (!status)
    {
        return maya_obj::ReportError("maya_obj: failed to assign mesh to default shading group", status);
    }

    return MS::kSuccess;
}

MStatus ObjSceneImporter::assignPerFaceMaterials(
    const rapidobj::Mesh &mesh,
    const rapidobj::Materials &materials,
    const MObject &meshTransformObj)
{
    std::map<int, std::vector<int>> materialFaceGroups;

    const int numFaces = static_cast<int>(mesh.num_face_vertices.size());
    for (int faceIdx = 0; faceIdx < numFaces; ++faceIdx)
    {
        const int matId = static_cast<int>(mesh.material_ids[faceIdx]);
        if (matId >= 0 && matId < static_cast<int>(materials.size()))
        {
            materialFaceGroups[matId].push_back(faceIdx);
        }
    }

    if (materialFaceGroups.empty())
    {
        return assignDefaultMaterial(meshTransformObj);
    }

    MDagPath meshDagPath;
    MDagPath::getAPathTo(meshTransformObj, meshDagPath);
    meshDagPath.extendToShape();

    for (const auto &entry : materialFaceGroups)
    {
        const int matId = entry.first;
        const rapidobj::Material &material = materials[matId];

        const std::string baseName = material.name.empty()
            ? "obj_material_" + std::to_string(matId)
            : material.name;

        const dcc_material::MaterialNodeNames materialNames =
            dcc_material::BuildMaterialNodeNames(baseName, "objMaterial");

        MObject shaderObject;
        MObject shadingGroupObject;
        MStatus status = dcc_material::EnsureSurfaceShaderBinding(
            "lambert",
            materialNames.shaderName,
            materialNames.shadingGroupName,
            shaderObject,
            shadingGroupObject);

        if (!status)
        {
            maya_obj::ReportWarning(
                MString("maya_obj: failed to ensure shader binding for material ") + baseName.c_str());
            continue;
        }

        MIntArray faceIndices;
        for (int faceIdx : entry.second)
        {
            faceIndices.append(faceIdx);
        }

        status = dcc_material::AssignFacesToShadingGroup(
            meshDagPath,
            faceIndices,
            shadingGroupObject);

        if (!status)
        {
            maya_obj::ReportWarning(
                MString("maya_obj: failed to assign faces to shading group for material ") + baseName.c_str());
        }
    }

    return MS::kSuccess;
}

MString ObjSceneImporter::sanitizeNodeName(const std::string &name) const
{
    if (name.empty())
    {
        return MString("obj_mesh");
    }

    std::string sanitized = name;
    for (char &c : sanitized)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
        {
            c = '_';
        }
    }

    if (!sanitized.empty() && std::isdigit(static_cast<unsigned char>(sanitized[0])))
    {
        sanitized = "obj_" + sanitized;
    }

    return MString(sanitized.c_str());
}
