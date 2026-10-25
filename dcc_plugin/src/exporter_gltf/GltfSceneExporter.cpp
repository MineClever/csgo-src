#include "GltfSceneExporter.h"

#include <common/ExportAnimationUtils.h>
#include <common/MaterialExportUtils.h>
#include <common/SkinClusterUtils.h>
#include <common_gltf/MayaGltfCommon.h>
#include <common_gltf/cgltf.h>
#define CGLTF_WRITE_IMPLEMENTATION
#include <common_gltf/cgltf_write.h>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFloatArray.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItGeometry.h>
#include <maya/MTime.h>
#include <maya/MQuaternion.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MMatrix.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItSelectionList.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace
{

// ---------------------------------------------------------------------------
// Append POD data to a byte buffer
// ---------------------------------------------------------------------------
template<typename T>
void AppendToBuffer(std::vector<unsigned char> &buffer, const T *data, size_t count)
{
    const size_t oldSize = buffer.size();
    buffer.resize(oldSize + count * sizeof(T));
    std::memcpy(buffer.data() + oldSize, data, count * sizeof(T));
}

// Align to 4 bytes
size_t Align4(size_t value)
{
    return (value + 3) & ~3;
}

// Duplicate a string with malloc (for cgltf compatibility with free())
char *MallocString(const std::string &s)
{
    char *mem = static_cast<char *>(std::malloc(s.size() + 1));
    if (mem)
    {
        std::memcpy(mem, s.c_str(), s.size() + 1);
    }
    return mem;
}

bool TryReadFloatPlug(MFnDependencyNode &node, const char *plugName, float &outValue)
{
    MStatus status;
    MPlug plug = node.findPlug(plugName, true, &status);
    if (!status)
    {
        return false;
    }

    outValue = plug.asFloat();
    return true;
}

bool TryReadColorPlug(MFnDependencyNode &node, const char *plugName, float *outColor)
{
    MStatus status;
    MPlug plug = node.findPlug(plugName, true, &status);
    if (!status || plug.numChildren() < 3)
    {
        return false;
    }

    outColor[0] = plug.child(0).asFloat();
    outColor[1] = plug.child(1).asFloat();
    outColor[2] = plug.child(2).asFloat();
    return true;
}

bool HasAnyNonZeroColor(const float *color)
{
    return color[0] > 0.0f || color[1] > 0.0f || color[2] > 0.0f;
}

constexpr const char *kGltfAlphaModeAttribute = "gltfAlphaMode";
constexpr const char *kGltfAlphaCutoffAttribute = "gltfAlphaCutoff";
constexpr const char *kGltfDoubleSidedAttribute = "gltfDoubleSided";
constexpr const char *kGltfOcclusionTextureAttribute = "gltfOcclusionTexture";

bool TryReadMeshBoolPlug(const MObject &meshObject, const char *plugName, bool &outValue)
{
    MStatus status;
    MFnDependencyNode meshNode(meshObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug plug = meshNode.findPlug(plugName, true, &status);
    if (!status)
    {
        return false;
    }

    outValue = plug.asBool(&status);
    return status;
}

bool TryResolveFileTexturePathFromPlugInternal(const MPlug &destinationPlug, std::string &texturePath);

bool TryResolveFileTexturePathFromNode(const MObject &nodeObject, std::string &texturePath)
{
    if (nodeObject.isNull())
    {
        return false;
    }

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return false;
    }

    const MString typeName = nodeFn.typeName(&status);
    if (!status)
    {
        return false;
    }

    if (typeName == "file")
    {
        MPlug fileTextureNamePlug = nodeFn.findPlug("fileTextureName", true, &status);
        if (!status)
        {
            return false;
        }

        const MString value = fileTextureNamePlug.asString(&status);
        if (!status || value.length() == 0)
        {
            return false;
        }

        texturePath = value.asChar();
        return true;
    }

    if (typeName == "reverse")
    {
        for (const char *inputPlugName : {"inputX", "inputY", "inputZ"})
        {
            MPlug inputPlug = nodeFn.findPlug(inputPlugName, true, &status);
            if (status && TryResolveFileTexturePathFromPlugInternal(inputPlug, texturePath))
            {
                return true;
            }
        }
        return false;
    }

    if (typeName == "bump2d")
    {
        MPlug bumpValuePlug = nodeFn.findPlug("bumpValue", true, &status);
        return status && TryResolveFileTexturePathFromPlugInternal(bumpValuePlug, texturePath);
    }

    MObject mutableNodeObject(nodeObject);
    MItDependencyGraph graphIt(
        mutableNodeObject,
        MFn::kDependencyNode,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return false;
    }

    for (; !graphIt.isDone(); graphIt.next())
    {
        const MObject upstreamObject = graphIt.thisNode(&status);
        if (!status || upstreamObject.isNull() || upstreamObject == nodeObject)
        {
            continue;
        }

        MFnDependencyNode upstreamFn(upstreamObject, &status);
        if (!status)
        {
            continue;
        }

        if (upstreamFn.typeName() != "file")
        {
            continue;
        }

        MPlug fileTextureNamePlug = upstreamFn.findPlug("fileTextureName", true, &status);
        if (!status)
        {
            continue;
        }

        const MString value = fileTextureNamePlug.asString(&status);
        if (!status || value.length() == 0)
        {
            continue;
        }

        texturePath = value.asChar();
        return true;
    }

    return false;
}

bool TryResolveFileTexturePathFromPlugInternal(const MPlug &destinationPlug, std::string &texturePath)
{
    texturePath.clear();
    if (destinationPlug.isNull())
    {
        return false;
    }

    MPlug mutableDestinationPlug(destinationPlug);
    MStatus status;
    MItDependencyGraph graphIt(
        mutableDestinationPlug,
        MFn::kDependencyNode,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return false;
    }

    for (; !graphIt.isDone(); graphIt.next())
    {
        const MObject upstreamObject = graphIt.thisNode(&status);
        if (!status || upstreamObject.isNull())
        {
            continue;
        }

        MFnDependencyNode upstreamFn(upstreamObject, &status);
        if (!status || upstreamFn.typeName() != "file")
        {
            continue;
        }

        MPlug fileTextureNamePlug = upstreamFn.findPlug("fileTextureName", true, &status);
        if (!status)
        {
            continue;
        }

        const MString value = fileTextureNamePlug.asString(&status);
        if (!status || value.length() == 0)
        {
            continue;
        }

        texturePath = value.asChar();
        return true;
    }

    return false;
}

bool TryResolveFileTexturePathFromPlug(MFnDependencyNode &node, const char *plugName, std::string &texturePath)
{
    MStatus status;
    MPlug plug = node.findPlug(plugName, true, &status);
    return status && TryResolveFileTexturePathFromPlugInternal(plug, texturePath);
}

MObject FindRenderableMeshChild(const MObject &transformObject)
{
    MStatus status;
    MFnTransform transformFn(transformObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    MObject fallbackMeshObject;
    for (unsigned int i = 0; i < transformFn.childCount(); ++i)
    {
        const MObject childObject = transformFn.child(i);
        if (!childObject.hasFn(MFn::kMesh))
        {
            continue;
        }

        if (fallbackMeshObject.isNull())
        {
            fallbackMeshObject = childObject;
        }

        MFnDependencyNode meshNodeFn(childObject, &status);
        if (!status)
        {
            continue;
        }

        MPlug intermediatePlug = meshNodeFn.findPlug("intermediateObject", true, &status);
        if (status && !intermediatePlug.asBool())
        {
            return childObject;
        }
    }

    return fallbackMeshObject;
}

} // anonymous namespace

// ============================================================================

GltfSceneExporter::GltfSceneExporter(
    const MFileObject &fileObject,
    const GltfExportOptions &exportOptions,
    MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , exportOptions_(exportOptions)
    , mode_(mode)
{
}

MStatus GltfSceneExporter::Export()
{
    MStatus status = collectMeshes();
    if (!status) return status;

    if (meshes_.empty())
    {
        maya_gltf::ReportWarning("maya_gltf: no meshes found to export");
        return MS::kSuccess;
    }

    // Collect animation data after all meshes and hierarchy are built
    status = collectAnimationData();
    if (!status)
    {
        maya_gltf::ReportWarning("maya_gltf: animation data collection completed with errors");
    }

    status = buildBufferData();
    if (!status) return status;

    const std::string outputPath = fileObject_.resolvedFullName().asChar();
    const bool isGlb = maya_gltf::HasGlbExtension(fileObject_);

    if (isGlb)
    {
        // GLB: buffer data is embedded — no external .bin needed
        status = buildDocument(nullptr);
    }
    else
    {
        // glTF: write external .bin file
        const std::string binPath = getBasename(outputPath) + ".bin";
        status = writeBufferFile(binPath);
        if (!status) return status;
        status = buildDocument(binPath.c_str());
    }
    if (!status) return status;

    // Write .gltf/.glb file
    cgltf_options writeOptions = {};
    if (isGlb)
    {
        writeOptions.type = cgltf_file_type_glb;
    }

    cgltf_result result = cgltf_write_file(&writeOptions, outputPath.c_str(), document_);
    safeFree();

    if (result != cgltf_result_success)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: failed to write glTF file: ") + outputPath.c_str());
    }

    maya_gltf::ReportInfo(
        MString("maya_gltf: exported ") + static_cast<int>(meshes_.size()) + " meshes to "
        + outputPath.c_str());

    return MS::kSuccess;
}

// ============================================================================
// Mesh Collection
// ============================================================================

MStatus GltfSceneExporter::collectMeshes()
{
    MSelectionList selList;

    if (mode_ == MPxFileTranslator::kExportActiveAccessMode)
    {
        MGlobal::getActiveSelectionList(selList);
    }

    if (selList.length() == 0)
    {
        selList.add("|", true);
    }

    std::set<std::string> visitedPaths;

    for (unsigned int i = 0; i < selList.length(); ++i)
    {
        MDagPath rootPath;
        if (!selList.getDagPath(i, rootPath)) continue;

        // 1. Collect hierarchy from this root
        MStatus hStatus = collectHierarchy(rootPath);
        if (!hStatus) return hStatus;

        // 2. Traverse all descendants to find mesh shapes
        MItDag dagIt(MItDag::kDepthFirst, MFn::kMesh);
        dagIt.reset(rootPath);

        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath meshDagPath;
            dagIt.getPath(meshDagPath);

            MDagPath transformPath = meshDagPath;
            transformPath.pop();

            const std::string pathStr = transformPath.fullPathName().asChar();
            if (visitedPaths.insert(pathStr).second)
            {
                // Find ancestor in hierarchy (prim transforms excluded)
                std::string ancestorPath;
                {
                    MDagPath ancestor = transformPath;
                    while (ancestor.length() > 0) {
                        std::string ap = ancestor.fullPathName().asChar();
                        if (dagPathToNodeIndex_.count(ap)) { ancestorPath = ap; break; }
                        if (ancestor.length() <= 1) break;
                        ancestor.pop();
                    }
                }
                if (!ancestorPath.empty()) {
                    int ai = dagPathToNodeIndex_[ancestorPath];
                    exportedNodes_[ai].meshIndex = (int)meshes_.size();
                }

                size_t oldMeshCount = meshes_.size();
                MStatus mStatus = collectMeshData(transformPath.transform());
                if (!mStatus) return mStatus;
                if (meshes_.size() > oldMeshCount) {
                    ExportedMesh &newMesh = meshes_.back();
                    newMesh.parentPath = ancestorPath;
                    // Collect skin data for this mesh
                    collectSkinData(transformPath.transform(), newMesh);
                }
            }
        }

        // 3. Traverse all descendants to find camera shapes
        if (exportOptions_.exportCamera)
        {
            MItDag cameraIt(MItDag::kDepthFirst, MFn::kCamera);
            cameraIt.reset(rootPath, MItDag::kDepthFirst, MFn::kCamera);

            for (; !cameraIt.isDone(); cameraIt.next())
            {
                MDagPath camDagPath;
                cameraIt.getPath(camDagPath);

                // Double-check API type to avoid false matches
                if (camDagPath.apiType() != MFn::kCamera)
                    continue;

                MDagPath parentPath = camDagPath;
                parentPath.pop();

                const std::string parentPathStr = parentPath.fullPathName().asChar();
                if (!dagPathToNodeIndex_.count(parentPathStr))
                    continue;

                // Skip Maya's default cameras (not part of the imported data)
                {
                    MString camName = camDagPath.partialPathName();
                    if (camName == "camera1" || camName == "persp" ||
                        camName == "top" || camName == "front" || camName == "side")
                        continue;
                }

                ExportedCamera cam;
                cam.name = camDagPath.partialPathName().asChar();
                cam.nodeDagPath = parentPathStr;

                MFnCamera fnCam(camDagPath);
                if (fnCam.isOrtho())
                {
                    cam.isPerspective = false;
                    cam.xmag = static_cast<float>(fnCam.orthoWidth());
                    cam.ymag = static_cast<float>(fnCam.orthoWidth());
                    cam.znear = static_cast<float>(fnCam.nearClippingPlane());
                    cam.zfar = static_cast<float>(fnCam.farClippingPlane());
                }
                else
                {
                    cam.isPerspective = true;
                    int resWidth = 1920, resHeight = 1080;
                    MSelectionList resSel;
                    if (resSel.add(":defaultResolution") == MS::kSuccess)
                    {
                        MObject resNode;
                        if (resSel.getDependNode(0, resNode) == MS::kSuccess)
                        {
                            MFnDependencyNode resFn(resNode);
                            MPlug wPlug = resFn.findPlug("width");
                            MPlug hPlug = resFn.findPlug("height");
                            if (!wPlug.isNull()) resWidth = wPlug.asInt();
                            if (!hPlug.isNull()) resHeight = hPlug.asInt();
                        }
                    }

                    double hFov, vFov;
                    fnCam.getPortFieldOfView(resWidth, resHeight, hFov, vFov);
                    cam.yfov = static_cast<float>(vFov);
                    cam.aspectRatio = (resHeight > 0) ? static_cast<float>(resWidth) / static_cast<float>(resHeight) : 0.0f;
                    cam.znear = static_cast<float>(fnCam.nearClippingPlane());
                    cam.zfar = static_cast<float>(fnCam.farClippingPlane());
                }

                cameras_.push_back(std::move(cam));
            }
        }
    }

    return MS::kSuccess;
}

MStatus GltfSceneExporter::collectMeshData(const MObject &transformObj)
{
    MStatus status;

    // Find mesh shape among children
    MFnTransform transformFn(transformObj);
    MObject meshObj = FindRenderableMeshChild(transformObj);

    if (meshObj.isNull()) return MS::kSuccess;

    MFnMesh meshFn(meshObj);
    ExportedMesh mesh;

    // Name
    MString name = transformFn.name();
    mesh.name = name.asChar();
    if (mesh.name.empty()) mesh.name = "mesh_" + std::to_string(meshes_.size());

    // ---- Vertex positions ----
    MPointArray mayaPoints;
    meshFn.getPoints(mayaPoints, MSpace::kObject);

    const unsigned int numVerts = mayaPoints.length();
    mesh.positions.resize(numVerts * 3);
    for (unsigned int v = 0; v < numVerts; ++v)
    {
        mesh.positions[v * 3 + 0] = static_cast<float>(mayaPoints[v].x);
        mesh.positions[v * 3 + 1] = static_cast<float>(mayaPoints[v].y);
        mesh.positions[v * 3 + 2] = static_cast<float>(mayaPoints[v].z);
    }

    // ---- Normals per-vertex (averaged from face-vertex normals) ----
    if (exportOptions_.exportNormals)
    {
    mesh.hasNormals = true;
    mesh.normals.resize(numVerts * 3, 0.0f);
    std::vector<int> vertNormalCount(numVerts, 0);

    MItMeshPolygon polyIt(meshObj);
    for (; !polyIt.isDone(); polyIt.next())
    {
        const int pvc = polyIt.polygonVertexCount();
        for (int v = 0; v < pvc; ++v)
        {
            int vertIdx = polyIt.vertexIndex(v);
            MVector normal;
            polyIt.getNormal(v, normal, MSpace::kObject);
            mesh.normals[vertIdx * 3 + 0] += static_cast<float>(normal.x);
            mesh.normals[vertIdx * 3 + 1] += static_cast<float>(normal.y);
            mesh.normals[vertIdx * 3 + 2] += static_cast<float>(normal.z);
            vertNormalCount[vertIdx]++;
        }
    }

    for (unsigned int v = 0; v < numVerts; ++v)
    {
        if (vertNormalCount[v] > 0)
        {
            float inv = 1.0f / static_cast<float>(vertNormalCount[v]);
            mesh.normals[v * 3 + 0] *= inv;
            mesh.normals[v * 3 + 1] *= inv;
            mesh.normals[v * 3 + 2] *= inv;
            float len = std::sqrt(
                mesh.normals[v * 3 + 0] * mesh.normals[v * 3 + 0] +
                mesh.normals[v * 3 + 1] * mesh.normals[v * 3 + 1] +
                mesh.normals[v * 3 + 2] * mesh.normals[v * 3 + 2]);
            if (len > 0.0001f)
            {
                mesh.normals[v * 3 + 0] /= len;
                mesh.normals[v * 3 + 1] /= len;
                mesh.normals[v * 3 + 2] /= len;
            }
        }
    }
    } // if exportNormals

    // ---- UV sets (all UV sets from Maya) ----
    {
        MStringArray uvSetNames;
        meshFn.getUVSetNames(uvSetNames);

        for (unsigned int s = 0; s < uvSetNames.length() && s < 8; ++s)
        {
            const MString &uvSetName = uvSetNames[s];

            MFloatArray uArray, vArray;
            meshFn.getUVs(uArray, vArray, &uvSetName);

            if (uArray.length() == 0 || vArray.length() == 0)
                continue;

            ExportedMesh::UVSet uvSet;
            uvSet.name = uvSetName.asChar();
            uvSet.data.resize(numVerts * 2, 0.0f);
            std::vector<int> vertUVCount(numVerts, 0);

            MItMeshPolygon uvPolyIt(meshObj);
            for (; !uvPolyIt.isDone(); uvPolyIt.next())
            {
                const int uvPvc = uvPolyIt.polygonVertexCount();
                for (int v = 0; v < uvPvc; ++v)
                {
                    int vertIdx = uvPolyIt.vertexIndex(v);
                    int uvIndex;
                    if (uvPolyIt.getUVIndex(v, uvIndex, &uvSetName))
                    {
                        uvSet.data[vertIdx * 2 + 0] += uArray[uvIndex];
                        uvSet.data[vertIdx * 2 + 1] += vArray[uvIndex];
                        vertUVCount[vertIdx]++;
                    }
                }
            }

            for (unsigned int v = 0; v < numVerts; ++v)
            {
                if (vertUVCount[v] > 0)
                {
                    float inv = 1.0f / static_cast<float>(vertUVCount[v]);
                    uvSet.data[v * 2 + 0] *= inv;
                    uvSet.data[v * 2 + 1] *= inv;
                    if (exportOptions_.flipUvV)
                    {
                        uvSet.data[v * 2 + 1] = 1.0f - uvSet.data[v * 2 + 1];
                    }
                }
            }

            mesh.uvSets.push_back(std::move(uvSet));
        }
    }

    // ---- Vertex colors (COLOR_0) ----
    {
        MString colorSetName("colorSet1");
        MColorArray vertColors;
        MStatus cStatus = meshFn.getVertexColors(vertColors, &colorSetName);
        if (cStatus && vertColors.length() > 0)
        {
            mesh.hasColors = true;
            mesh.colorComponents = 4;
            mesh.colors.resize(numVerts * 4, 1.0f);

            // Map color index to vertex index (same layout since we use getPoints)
            for (unsigned int v = 0; v < vertColors.length() && v < numVerts; ++v)
            {
                mesh.colors[v * 4 + 0] = vertColors[v].r;
                mesh.colors[v * 4 + 1] = vertColors[v].g;
                mesh.colors[v * 4 + 2] = vertColors[v].b;
                mesh.colors[v * 4 + 3] = vertColors[v].a;
            }
        }
    }

    // ---- Triangle indices ----
    // Maya 2022: getTriangles(MIntArray& triangleCounts, MIntArray& triangleVertices)
    MIntArray triCounts;
    MIntArray triConnects;
    meshFn.getTriangles(triCounts, triConnects);

    const unsigned int numTris = triConnects.length();
    mesh.indices.reserve(numTris);
    for (unsigned int t = 0; t < numTris; ++t)
    {
        mesh.indices.push_back(static_cast<uint32_t>(triConnects[t]));
    }

    // ---- Material ----
    {
        ExportedMesh::ExportedMaterial mat;
        mat.name = mesh.name + "_mat";
        collectMaterial(transformObj, mat);
        mesh.material = mat;
    }

    meshes_.push_back(std::move(mesh));
    return MS::kSuccess;
}

// ============================================================================
// Material Collection
// ============================================================================

MStatus GltfSceneExporter::collectMaterial(
    const MObject &transformObj, ExportedMesh::ExportedMaterial &outMat)
{
    MStatus status;

    // Find mesh shape among children
    MObject meshObj = FindRenderableMeshChild(transformObj);

    if (meshObj.isNull()) return MS::kSuccess;

    // Use MaterialExportUtils to get shading assignments
    MFnMesh meshFn(meshObj);
    dcc_material_export::MeshShadingAssignments shadingInfo;
    if (!dcc_material_export::GetMeshShadingAssignments(meshFn, 0, shadingInfo))
    {
        return MS::kSuccess;
    }

    for (unsigned int i = 0; i < shadingInfo.shadingGroups.length(); ++i)
    {
        const MObject &sgObj = shadingInfo.shadingGroups[i];

        // Find the actual shader node
        MObject shaderObj = dcc_material_export::FindSurfaceShaderNode(sgObj);
        if (shaderObj.isNull())
        {
            continue;
        }

        MFnDependencyNode shaderNode(shaderObj, &status);
        if (!status)
        {
            continue;
        }

        const std::string shaderType = shaderNode.typeName().asChar();
        const std::string shaderName = shaderNode.name().asChar();
        if (!shaderName.empty())
        {
            outMat.name = shaderName;
        }

        if (!TryReadColorPlug(shaderNode, "baseColor", outMat.baseColorFactor))
        {
            TryReadColorPlug(shaderNode, "color", outMat.baseColorFactor);
        }

        float opacity[3] = {1.0f, 1.0f, 1.0f};
        if (TryReadColorPlug(shaderNode, "opacity", opacity))
        {
            outMat.baseColorFactor[3] = (opacity[0] + opacity[1] + opacity[2]) / 3.0f;
        }
        else
        {
            MPlug transPlug = shaderNode.findPlug("transparency", &status);
            if (status && transPlug.numChildren() >= 3)
            {
                const float t = (transPlug.child(0).asFloat() +
                                 transPlug.child(1).asFloat() +
                                 transPlug.child(2).asFloat()) / 3.0f;
                outMat.baseColorFactor[3] = 1.0f - t;
            }
        }

        if (TryReadColorPlug(shaderNode, "emissionColor", outMat.emissiveFactor))
        {
            float emissionWeight = 1.0f;
            if (TryReadFloatPlug(shaderNode, "emission", emissionWeight))
            {
                outMat.emissiveFactor[0] *= emissionWeight;
                outMat.emissiveFactor[1] *= emissionWeight;
                outMat.emissiveFactor[2] *= emissionWeight;
            }
        }
        else
        {
            MPlug incPlug = shaderNode.findPlug("incandescence", &status);
            if (status && incPlug.numChildren() >= 3)
            {
                outMat.emissiveFactor[0] = incPlug.child(0).asFloat();
                outMat.emissiveFactor[1] = incPlug.child(1).asFloat();
                outMat.emissiveFactor[2] = incPlug.child(2).asFloat();
            }
        }

        if (!TryReadFloatPlug(shaderNode, "metalness", outMat.metallicFactor))
        {
            outMat.metallicFactor = 0.0f;
        }

        if (!TryReadFloatPlug(shaderNode, "specularRoughness", outMat.roughnessFactor))
        {
            if (shaderType == "lambert" || shaderType == "Lambert")
            {
                outMat.roughnessFactor = 1.0f;
            }
            else
            {
                outMat.roughnessFactor = 0.5f;
            }
        }

        TryReadFloatPlug(shaderNode, "specular", outMat.specularFactor);
        TryReadColorPlug(shaderNode, "specularColor", outMat.specularColorFactor);
        TryReadFloatPlug(shaderNode, "specularIOR", outMat.ior);
        TryReadFloatPlug(shaderNode, "transmission", outMat.transmissionFactor);
        TryReadFloatPlug(shaderNode, "coat", outMat.clearcoatFactor);
        TryReadFloatPlug(shaderNode, "coatRoughness", outMat.clearcoatRoughnessFactor);
        TryReadColorPlug(shaderNode, "sheenColor", outMat.sheenColorFactor);
        TryReadFloatPlug(shaderNode, "sheenRoughness", outMat.sheenRoughnessFactor);

        TryResolveFileTexturePathFromPlug(shaderNode, "baseColor", outMat.baseColorTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "metalness", outMat.metallicRoughnessTexturePath);
        if (outMat.metallicRoughnessTexturePath.empty())
        {
            TryResolveFileTexturePathFromPlug(shaderNode, "specularRoughness", outMat.metallicRoughnessTexturePath);
        }
        TryResolveFileTexturePathFromPlug(shaderNode, "normalCamera", outMat.normalTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "emissionColor", outMat.emissiveTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "specular", outMat.specularTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "specularColor", outMat.specularColorTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "transmission", outMat.transmissionTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "coat", outMat.clearcoatTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "coatRoughness", outMat.clearcoatRoughnessTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "coatNormal", outMat.clearcoatNormalTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "sheenColor", outMat.sheenColorTexturePath);
        TryResolveFileTexturePathFromPlug(shaderNode, "sheenRoughness", outMat.sheenRoughnessTexturePath);

        std::string alphaModeString;
        if (dcc_material_export::ReadStringAttribute(shaderObj, kGltfAlphaModeAttribute, alphaModeString))
        {
            if (alphaModeString == "MASK")
            {
                outMat.alphaMode = cgltf_alpha_mode_mask;
            }
            else if (alphaModeString == "BLEND")
            {
                outMat.alphaMode = cgltf_alpha_mode_blend;
            }
            else
            {
                outMat.alphaMode = cgltf_alpha_mode_opaque;
            }
        }

        dcc_material_export::ReadFloatAttribute(shaderObj, kGltfAlphaCutoffAttribute, outMat.alphaCutoff);
        dcc_material_export::ReadStringAttribute(shaderObj, kGltfOcclusionTextureAttribute, outMat.occlusionTexturePath);
        TryReadMeshBoolPlug(meshObj, "doubleSided", outMat.doubleSided);
        dcc_material_export::ReadBoolAttribute(shaderObj, kGltfDoubleSidedAttribute, outMat.doubleSided);

        if (outMat.alphaMode != cgltf_alpha_mode_mask)
        {
            outMat.alphaMode = outMat.baseColorFactor[3] < 1.0f
                ? cgltf_alpha_mode_blend
                : outMat.alphaMode;
        }

        outMat.hasPBR = true;
        break; // first shader only
    }

    return MS::kSuccess;
}

// ============================================================================
// Skinning Data Collection
// ============================================================================

MStatus GltfSceneExporter::collectSkinData(const MObject &transformObj, ExportedMesh &mesh)
{
    if (!exportOptions_.exportSkin)
        return MS::kSuccess;

    MStatus status;

    // Find mesh shape among children
    MFnTransform transformFn(transformObj);
    MObject meshObj;
    for (unsigned int i = 0; i < transformFn.childCount(); ++i)
    {
        MObject child = transformFn.child(i);
        if (child.hasFn(MFn::kMesh))
        {
            meshObj = child;
            break;
        }
    }
    if (meshObj.isNull()) return MS::kSuccess;

    // Look for skinCluster upstream from this mesh
    MItDependencyGraph depIt(meshObj, MFn::kSkinClusterFilter, MItDependencyGraph::kUpstream);
    if (depIt.isDone()) return MS::kSuccess; // no skin cluster

    MObject skinClusterObj = depIt.thisNode();
    if (skinClusterObj.isNull()) return MS::kSuccess;

    MFnSkinCluster skinClusterFn(skinClusterObj);

    // Get influence objects (joints)
    MDagPathArray influencePaths;
    skinClusterFn.influenceObjects(influencePaths);
    const unsigned int numJoints = influencePaths.length();
    if (numJoints == 0) return MS::kSuccess;

    // Get mesh DAG path
    MDagPath meshDagPath;
    MDagPath::getAPathTo(meshObj, meshDagPath);

    const unsigned int numVerts = static_cast<unsigned int>(mesh.positions.size() / 3);
    const unsigned int maxInfluences = 4;

    // ---- Fill joint info (dag paths + inverse bind matrices) ----
    mesh.joints.clear();
    mesh.joints.reserve(numJoints);
    for (unsigned int j = 0; j < numJoints; ++j)
    {
        ExportedMesh::SkinJoint joint;
        joint.dagPath = influencePaths[j].fullPathName().asChar();

        // Default to identity matrix in glTF column-major layout.
        for (int k = 0; k < 16; ++k)
            joint.inverseBindMatrix[k] = (k % 5 == 0) ? 1.0f : 0.0f;

        // Read bindPreMatrix for this influence using the logical index Maya
        // assigned to the influence object rather than assuming sequential
        // logical indices.
        MPlug bindPlug = skinClusterFn.findPlug("bindPreMatrix", &status);
        if (status && !bindPlug.isNull())
        {
            const unsigned int logicalIndex = skinClusterFn.indexForInfluenceObject(influencePaths[j], &status);
            if (status)
            {
                MPlug elemPlug = bindPlug.elementByLogicalIndex(logicalIndex, &status);
                if (!elemPlug.isNull())
                {
                    MObject matrixObj;
                    if (elemPlug.getValue(matrixObj) == MS::kSuccess && !matrixObj.isNull())
                    {
                        MFnMatrixData matrixData(matrixObj);
                        MMatrix m = matrixData.matrix();
                        for (int r = 0; r < 4; ++r)
                            for (int c = 0; c < 4; ++c)
                                joint.inverseBindMatrix[c * 4 + r] = static_cast<float>(m[r][c]);
                    }
                }
            }
            status = MS::kSuccess;
        }

        mesh.joints.push_back(joint);
    }

    // ---- Pre-allocate joint indices and weights (maxInfluences per vertex) ----
    mesh.jointIndices.resize(numVerts * maxInfluences, 0);
    mesh.jointWeights.resize(numVerts * maxInfluences, 0.0f);
    mesh.jointsPerVertex = maxInfluences;

    // ---- Get per-vertex weights from skinCluster ----
    MObject vertexComponent;
    dcc_skinning::CreateMeshVertexComponent(numVerts, vertexComponent);

    MIntArray infIndices;
    MDoubleArray weights;
    status = skinClusterFn.getWeights(meshDagPath, vertexComponent, infIndices, weights);
    if (!status)
    {
        return MS::kSuccess; // unable to read weights, partial skin data ok
    }

    const unsigned int numInf = static_cast<unsigned int>(infIndices.length());

    // For each vertex, extract the top N weights
    for (unsigned int vi = 0; vi < numVerts && (vi * numInf) < weights.length(); ++vi)
    {
        std::vector<std::pair<float, unsigned int>> sortedWeights;
        sortedWeights.reserve(numInf);

        for (unsigned int inf = 0; inf < numInf; ++inf)
        {
            const double w = weights[vi * numInf + inf];
            if (w > 0.001) // threshold out negligible weights
            {
                sortedWeights.push_back({ static_cast<float>(w), static_cast<unsigned int>(infIndices[inf]) });
            }
        }

        // Sort by weight descending
        std::sort(sortedWeights.begin(), sortedWeights.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

        // Take top maxInfluences
        const int count = std::min(static_cast<int>(sortedWeights.size()), static_cast<int>(maxInfluences));
        float totalW = 0.0f;
        for (int k = 0; k < count; ++k)
        {
            mesh.jointIndices[vi * maxInfluences + k] = static_cast<uint16_t>(sortedWeights[k].second);
            mesh.jointWeights[vi * maxInfluences + k] = sortedWeights[k].first;
            totalW += sortedWeights[k].first;
        }

        // Renormalize top N weights to sum to 1.0
        if (totalW > 0.001f)
        {
            const float invTotal = 1.0f / totalW;
            for (int k = 0; k < count; ++k)
                mesh.jointWeights[vi * maxInfluences + k] *= invTotal;
        }
    }

    mesh.hasSkin = true;
    return MS::kSuccess;
}

// ============================================================================
// Hierarchy Collection
// ============================================================================

int GltfSceneExporter::getOrCreateNode(const std::string &dagPath, const std::string &name, int parentIndex)
{
    auto it = dagPathToNodeIndex_.find(dagPath);
    if (it != dagPathToNodeIndex_.end())
        return it->second;

    ExportedNode node;
    node.dagPath = dagPath;
    node.name = name;
    node.parentIndex = parentIndex;
    int idx = static_cast<int>(exportedNodes_.size());
    exportedNodes_.push_back(node);
    dagPathToNodeIndex_[dagPath] = idx;
    return idx;
}

MStatus GltfSceneExporter::collectHierarchy(const MDagPath &rootPath)
{
    // Traverse all transforms under rootPath (depth-first) and record them.
    // Skip "prim transforms" that directly own a mesh shape — these are
    // internal importer artifacts and should not create extra glTF nodes.
    MItDag dagIt(MItDag::kDepthFirst, MFn::kTransform);
    dagIt.reset(rootPath);

    for (; !dagIt.isDone(); dagIt.next())
    {
        MDagPath nodePath;
        dagIt.getPath(nodePath);

        // Skip if this transform has a direct mesh shape child (prim transform)
        {
            MObject transformObj = nodePath.transform();
            MFnDagNode dagNode(transformObj);
            bool hasDirectMesh = false;
            for (unsigned int ci = 0; ci < dagNode.childCount(); ++ci)
            {
                MObject child = dagNode.child(ci);
                if (child.hasFn(MFn::kMesh))
                {
                    hasDirectMesh = true;
                    break;
                }
            }
            if (hasDirectMesh) continue;
        }

        const std::string fullPath = nodePath.fullPathName().asChar();
        const std::string nodeName = nodePath.partialPathName().asChar();

        // Find parent dag path
        std::string parentPath;
        int parentIndex = -1;
        if (nodePath.length() > 1)
        {
            MDagPath parentDag = nodePath;
            parentDag.pop();
            parentPath = parentDag.fullPathName().asChar();
            parentIndex = getOrCreateNode(parentPath,
                parentDag.partialPathName().asChar(), -1);
        }

        int nodeIdx = getOrCreateNode(fullPath, nodeName, parentIndex);
        if (nodeIdx < 0) continue;

        ExportedNode &node = exportedNodes_[nodeIdx];
        node.parentIndex = parentIndex;

        // Extract TRS from the transform
        MObject transformObj = nodePath.transform();
        if (!transformObj.isNull())
        {
            MFnTransform fnTransform(transformObj);
            MTransformationMatrix tm = fnTransform.transformation();

            MVector t = tm.translation(MSpace::kTransform);
            node.translation[0] = static_cast<float>(t.x);
            node.translation[1] = static_cast<float>(t.y);
            node.translation[2] = static_cast<float>(t.z);
            node.hasTranslation = true;

            MQuaternion q = tm.rotation();
            node.rotation[0] = static_cast<float>(q.x);
            node.rotation[1] = static_cast<float>(q.y);
            node.rotation[2] = static_cast<float>(q.z);
            node.rotation[3] = static_cast<float>(q.w);
            node.hasRotation = true;

            double s[3];
            tm.getScale(s, MSpace::kTransform);
            node.scale[0] = static_cast<float>(s[0]);
            node.scale[1] = static_cast<float>(s[1]);
            node.scale[2] = static_cast<float>(s[2]);
            node.hasScale = true;
        }
    }

    return MS::kSuccess;
}

// ============================================================================
// Animation Data Collection
// ============================================================================

MStatus GltfSceneExporter::collectAnimationData()
{
    if (!exportOptions_.exportAnimation)
        return MS::kSuccess;

    MStatus status;

    // Iterate exported hierarchy nodes to find animated transforms
    for (const auto &[dagPath, nodeIdx] : dagPathToNodeIndex_)
    {
        const ExportedNode &en = exportedNodes_[nodeIdx];
        if (en.gltfNodeIndex < 0) continue; // skip flat mesh nodes

        MSelectionList sel;
        sel.add(MString(dagPath.c_str()));
        MDagPath nodeDagPath;
        if (sel.getDagPath(0, nodeDagPath) != MS::kSuccess) continue;

        MObject transformObj = nodeDagPath.transform();
        if (transformObj.isNull()) continue;

        MFnDependencyNode depFn(transformObj);

        // Check if this node has animation curves
        dcc_animation_export::TransformSampleSet samples;
        if (!dcc_animation_export::BuildTransformSampleSet(depFn, samples))
            continue; // no animation curves

        // Verify at least one channel actually has curves
        bool hasAny = false;
        for (int i = 0; i < 3 && !hasAny; ++i)
            hasAny = (!samples.translation[i].curves.empty());
        for (int i = 0; i < 3 && !hasAny; ++i)
            hasAny = (!samples.rotation[i].curves.empty());
        if (!hasAny) continue;

        // Collect all keyframe times
        std::vector<double> times;
        dcc_animation_export::AppendTransformSampleTimes(samples, times, MTime::kSeconds);
        if (times.empty()) continue;

        // Sort and deduplicate times
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end(),
            [](double a, double b) { return std::abs(a - b) < 0.0001; }), times.end());

        const size_t numSamples = times.size();

        SampledAnimation sa;
        sa.targetNodeGltfIndex = en.gltfNodeIndex;

        // ---- Translation channel (if animated) ----
        bool hasTrans = false;
        for (int i = 0; i < 3; ++i)
            if (!samples.translation[i].curves.empty()) hasTrans = true;

        if (hasTrans)
        {
            // Input data (times)
            sa.inputData.reserve(sa.inputData.size() + numSamples);
            for (double t : times)
                sa.inputData.push_back(static_cast<float>(t));

            // Output data (sampled translation values)
            sa.outputData.reserve(sa.outputData.size() + numSamples * 3);

            for (size_t ti = 0; ti < numSamples; ++ti)
            {
                MTime mt(times[ti], MTime::kSeconds);
                MAnimControl::setCurrentTime(mt);

                MFnTransform fnTransform(transformObj);
                MTransformationMatrix tm = fnTransform.transformation();
                MVector t = tm.translation(MSpace::kTransform);

                sa.outputData.push_back(static_cast<float>(t.x));
                sa.outputData.push_back(static_cast<float>(t.y));
                sa.outputData.push_back(static_cast<float>(t.z));
            }

            // Input buffer view + accessor
            {
                cgltf_buffer_view bv = {};
                bv.size = numSamples * sizeof(float);
                bv.stride = sizeof(float);
                bv.type = cgltf_buffer_view_type_vertices;
                sa.inputBufferViews.push_back(bv);

                cgltf_accessor acc = {};
                acc.count = numSamples;
                acc.type = cgltf_type_scalar;
                acc.component_type = cgltf_component_type_r_32f;
                acc.stride = sizeof(float);
                sa.inputAccessors.push_back(acc);
            }

            // Output buffer view + accessor
            {
                cgltf_buffer_view bv = {};
                bv.size = numSamples * 3 * sizeof(float);
                bv.stride = sizeof(float) * 3;
                bv.type = cgltf_buffer_view_type_vertices;
                sa.outputBufferViews.push_back(bv);

                cgltf_accessor acc = {};
                acc.count = numSamples;
                acc.type = cgltf_type_vec3;
                acc.component_type = cgltf_component_type_r_32f;
                acc.stride = sizeof(float) * 3;
                sa.outputAccessors.push_back(acc);
            }

            // Sampler
            cgltf_animation_sampler samp = {};
            samp.interpolation = cgltf_interpolation_type_linear;
            samp.input = &sa.inputAccessors.back();
            samp.output = &sa.outputAccessors.back();
            sa.samplers.push_back(samp);

            // Channel
            cgltf_animation_channel chan = {};
            chan.target_path = cgltf_animation_path_type_translation;
            chan.sampler = &sa.samplers.back();
            sa.channels.push_back(chan);
        }

        // ---- Rotation channel (if animated) ----
        bool hasRot = false;
        for (int i = 0; i < 3; ++i)
            if (!samples.rotation[i].curves.empty()) hasRot = true;

        if (hasRot)
        {
            sa.inputData.reserve(sa.inputData.size() + numSamples);
            for (double t : times)
                sa.inputData.push_back(static_cast<float>(t));

            sa.outputData.reserve(sa.outputData.size() + numSamples * 4);

            for (size_t ti = 0; ti < numSamples; ++ti)
            {
                MTime mt(times[ti], MTime::kSeconds);
                MAnimControl::setCurrentTime(mt);

                MFnTransform fnTransform(transformObj);
                MTransformationMatrix tm = fnTransform.transformation();
                MQuaternion q = tm.rotation();

                sa.outputData.push_back(static_cast<float>(q.x));
                sa.outputData.push_back(static_cast<float>(q.y));
                sa.outputData.push_back(static_cast<float>(q.z));
                sa.outputData.push_back(static_cast<float>(q.w));
            }

            // Input buffer view + accessor
            {
                cgltf_buffer_view bv = {};
                bv.size = numSamples * sizeof(float);
                bv.stride = sizeof(float);
                bv.type = cgltf_buffer_view_type_vertices;
                sa.inputBufferViews.push_back(bv);

                cgltf_accessor acc = {};
                acc.count = numSamples;
                acc.type = cgltf_type_scalar;
                acc.component_type = cgltf_component_type_r_32f;
                acc.stride = sizeof(float);
                sa.inputAccessors.push_back(acc);
            }

            // Output buffer view + accessor
            {
                cgltf_buffer_view bv = {};
                bv.size = numSamples * 4 * sizeof(float);
                bv.stride = sizeof(float) * 4;
                bv.type = cgltf_buffer_view_type_vertices;
                sa.outputBufferViews.push_back(bv);

                cgltf_accessor acc = {};
                acc.count = numSamples;
                acc.type = cgltf_type_vec4;
                acc.component_type = cgltf_component_type_r_32f;
                acc.stride = sizeof(float) * 4;
                sa.outputAccessors.push_back(acc);
            }

            cgltf_animation_sampler samp = {};
            samp.interpolation = cgltf_interpolation_type_linear;
            samp.input = &sa.inputAccessors.back();
            samp.output = &sa.outputAccessors.back();
            sa.samplers.push_back(samp);

            cgltf_animation_channel chan = {};
            chan.target_path = cgltf_animation_path_type_rotation;
            chan.sampler = &sa.samplers.back();
            sa.channels.push_back(chan);
        }

        // Wire up the animation
        sa.glAnim.name = nullptr;
        sa.glAnim.channels = sa.channels.data();
        sa.glAnim.channels_count = sa.channels.size();
        sa.glAnim.samplers = sa.samplers.data();
        sa.glAnim.samplers_count = sa.samplers.size();

        sampledAnimations_.push_back(std::move(sa));
    }

    // Restore current time to start of timeline
    MAnimControl::setCurrentTime(MAnimControl::minTime());

    return MS::kSuccess;
}

// ============================================================================
// Buffer Building
// ============================================================================

MStatus GltfSceneExporter::buildBufferData()
{
    bufferData_.clear();

    for (const auto &mesh : meshes_)
    {
        // Align offset
        bufferData_.resize(Align4(bufferData_.size()), 0);

        // Positions
        AppendToBuffer(bufferData_, mesh.positions.data(), mesh.positions.size());

        // Normals (if present)
        if (mesh.hasNormals)
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, mesh.normals.data(), mesh.normals.size());
        }

        // UV sets (0..N)
        for (const auto &uvSet : mesh.uvSets)
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, uvSet.data.data(), uvSet.data.size());
        }

        // Vertex colors (if present)
        if (mesh.hasColors)
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, mesh.colors.data(), mesh.colors.size());
        }

        // Indices
        bufferData_.resize(Align4(bufferData_.size()), 0);
        AppendToBuffer(bufferData_, mesh.indices.data(), mesh.indices.size());

        // Skin joint indices (JOINTS_0) and weights (WEIGHTS_0)
        if (mesh.hasSkin)
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, mesh.jointIndices.data(), mesh.jointIndices.size());

            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, mesh.jointWeights.data(), mesh.jointWeights.size());
        }
    }

    // Append inverse bind matrix data for all skins
    // Each joint contributes 16 floats (4x4 matrix column-major)
    for (const auto &mesh : meshes_)
    {
        if (!mesh.hasSkin) continue;
        for (const auto &joint : mesh.joints)
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, joint.inverseBindMatrix, 16);
        }
    }

    // Append animation input/output data
    for (const auto &sa : sampledAnimations_)
    {
        if (!sa.inputData.empty())
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, sa.inputData.data(), sa.inputData.size());
        }
        if (!sa.outputData.empty())
        {
            bufferData_.resize(Align4(bufferData_.size()), 0);
            AppendToBuffer(bufferData_, sa.outputData.data(), sa.outputData.size());
        }
    }

    return MS::kSuccess;
}

MStatus GltfSceneExporter::writeBufferFile(const std::string &binPath)
{
    std::ofstream out(binPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: failed to write buffer file: ") + binPath.c_str());
    }
    out.write(reinterpret_cast<const char *>(bufferData_.data()), bufferData_.size());
    out.close();
    return MS::kSuccess;
}

// ============================================================================
// cgltf_data Construction
// ============================================================================

MStatus GltfSceneExporter::buildDocument(const char *binURIPath)
{
    const size_t numMeshes = meshes_.size();
    if (numMeshes == 0) return MS::kSuccess;

    // Determine buffer URI or null for GLB (embedded)
    std::string binFilename;
    const bool isGlb = (binURIPath == nullptr);
    if (!isGlb)
    {
        binFilename = std::filesystem::path(binURIPath).filename().string();
    }

    // Free previous strings
    for (char *s : ownedStrings_) std::free(s);
    ownedStrings_.clear();

    const std::filesystem::path exportPath(fileObject_.resolvedFullName().asChar());
    const std::filesystem::path exportDirectory = exportPath.parent_path();

    auto normalizeTextureUri = [&](const std::string &texturePath) -> std::string
    {
        if (texturePath.empty())
        {
            return {};
        }

        std::filesystem::path sourcePath(texturePath);
        std::filesystem::path relativePath = sourcePath;
        std::error_code ec;
        if (sourcePath.is_absolute())
        {
            relativePath = std::filesystem::relative(sourcePath, exportDirectory, ec);
            if (ec || relativePath.empty())
            {
                relativePath = sourcePath.filename();
            }
        }

        return relativePath.generic_string();
    };

    std::vector<std::string> uniqueTextureUris;
    std::map<std::string, size_t> textureIndexByUri;
    auto registerTexturePath = [&](const std::string &texturePath)
    {
        const std::string uri = normalizeTextureUri(texturePath);
        if (uri.empty() || textureIndexByUri.find(uri) != textureIndexByUri.end())
        {
            return;
        }

        textureIndexByUri[uri] = uniqueTextureUris.size();
        uniqueTextureUris.push_back(uri);
    };

    for (const auto &mesh : meshes_)
    {
        const auto &mat = mesh.material;
        registerTexturePath(mat.baseColorTexturePath);
        registerTexturePath(mat.metallicRoughnessTexturePath);
        registerTexturePath(mat.normalTexturePath);
        registerTexturePath(mat.occlusionTexturePath);
        registerTexturePath(mat.emissiveTexturePath);
        registerTexturePath(mat.specularTexturePath);
        registerTexturePath(mat.specularColorTexturePath);
        registerTexturePath(mat.transmissionTexturePath);
        registerTexturePath(mat.clearcoatTexturePath);
        registerTexturePath(mat.clearcoatRoughnessTexturePath);
        registerTexturePath(mat.clearcoatNormalTexturePath);
        registerTexturePath(mat.sheenColorTexturePath);
        registerTexturePath(mat.sheenRoughnessTexturePath);
    }

    // ---- Pre-count totals (to pre-allocate vectors so pointers remain stable) ----
    size_t totalBufferViews = 0;
    size_t totalAccessors = 0;
    size_t totalAttributes = 0;
    size_t totalPrimitives = numMeshes;

    for (const auto &mesh : meshes_)
    {
        totalBufferViews += 1; // positions bufView
        if (mesh.hasNormals) totalBufferViews += 1;
        totalBufferViews += mesh.uvSets.size(); // each UV set
        if (mesh.hasColors) totalBufferViews += 1;
        totalBufferViews += 1; // indices bufView

        totalAccessors += 1; // positions
        if (mesh.hasNormals) totalAccessors += 1;
        totalAccessors += mesh.uvSets.size(); // each UV set
        if (mesh.hasColors) totalAccessors += 1;
        totalAccessors += 1; // indices

        totalAttributes += 1; // POSITION
        if (mesh.hasNormals) totalAttributes += 1;
        totalAttributes += mesh.uvSets.size(); // TEXCOORD_0..N
        if (mesh.hasColors) totalAttributes += 1; // COLOR_0

        // Skin attributes (JOINTS_0 + WEIGHTS_0)
        if (mesh.hasSkin)
        {
            totalBufferViews += 2;
            totalAccessors += 2;
            totalAttributes += 2;
        }
    }

    // Count skins (IBM accessors stored in separate flat arrays)
    size_t totalSkins = 0;
    size_t totalIbmMatrices = 0; // total float[16] entries across all skins
    for (const auto &mesh : meshes_)
    {
        if (mesh.hasSkin)
        {
            totalSkins++;
            totalIbmMatrices += mesh.joints.size();
        }
    }

    // Count animation channels, samplers, accessors, buffer views
    size_t totalAnimChannels = 0;
    size_t totalAnimSamplers = 0;
    size_t totalAnimInputAccessors = 0;
    size_t totalAnimOutputAccessors = 0;
    size_t totalAnimInputBV = 0;
    size_t totalAnimOutputBV = 0;
    for (const auto &sa : sampledAnimations_)
    {
        totalAnimChannels += sa.channels.size();
        totalAnimSamplers += sa.samplers.size();
        totalAnimInputAccessors += sa.inputAccessors.size();
        totalAnimOutputAccessors += sa.outputAccessors.size();
        totalAnimInputBV += sa.inputBufferViews.size();
        totalAnimOutputBV += sa.outputBufferViews.size();
    }

    // Add IBM buffer views + accessors (skin level, stored in separate skin arrays)
    // These are NOT added to the main flatBufferViews_/flatAccessors_ — they go
    // into flatSkinBufferViews_ and flatSkinAccessors_
    // Add IBM accessors and buffer views to main totals (required for cgltf serialization)
    totalBufferViews += totalSkins;
    totalAccessors += totalSkins;

    // Pre-allocate
    flatBuffers_.resize(1);
    flatBufferViews_.resize(totalBufferViews);
    flatAccessors_.resize(totalAccessors);
    flatAttributes_.resize(totalAttributes);
    flatPrimitives_.resize(totalPrimitives);
    flatMeshes_.resize(numMeshes);
    flatMaterials_.resize(numMeshes);
    flatTextures_.resize(uniqueTextureUris.size());
    flatImages_.resize(uniqueTextureUris.size());
    flatSkins_.resize(totalSkins);
    flatSkinJointArrays_.resize(totalSkins); // pre-allocate to avoid realloc
    flatAnimations_.resize(sampledAnimations_.size());
    flatAnimChannels_.resize(totalAnimChannels);
    flatAnimSamplers_.resize(totalAnimSamplers);

    // Count hierarchy nodes. Meshes attach to their nearest hierarchy ancestor
    // (skipping prim transforms). Root-level hierarchy nodes that have a mesh
    // are "simple" — we treat them as flat mesh nodes instead to avoid creating
    // extra transform nodes in the reimport.
    size_t flatMeshNodes = 0;
    for (size_t m = 0; m < numMeshes; ++m)
    {
        bool hasParent = false;
        for (auto &en : exportedNodes_)
        {
            if (en.meshIndex == static_cast<int>(m) && en.parentIndex >= 0)
            {
                hasParent = true;
                break;
            }
        }
        if (!hasParent) flatMeshNodes++;
    }
    // Flat mesh count + remaining hierarchy nodes
    size_t hierNodesToCreate = 0;
    for (auto &en : exportedNodes_)
    {
        if (en.meshIndex >= 0 && en.parentIndex < 0)
        {
            // Root-level node with mesh → mesh is flat, no hierarchy node needed
            en.gltfNodeIndex = -1;
        }
        else
        {
            en.gltfNodeIndex = static_cast<int>(hierNodesToCreate);
            hierNodesToCreate++;
        }
    }
    const size_t totalNodes = hierNodesToCreate + flatMeshNodes;
    flatNodes_.resize(totalNodes);
    flatSceneNodes_.resize(flatMeshNodes);
    std::vector<int> rootNodeIndices;

    // Build hierarchy nodes
    for (auto &src : exportedNodes_)
    {
        if (src.gltfNodeIndex < 0) continue; // flat mesh, no hierarchy node
        int hi = src.gltfNodeIndex;
        cgltf_node &node = flatNodes_[hi];
        node = {};
        ownedStrings_.push_back(MallocString(src.name));
        node.name = ownedStrings_.back();
        src.gltfNodeIndex = static_cast<int>(hi);

        if (src.hasTranslation) { node.has_translation = true;
            node.translation[0] = src.translation[0]; node.translation[1] = src.translation[1]; node.translation[2] = src.translation[2]; }
        if (src.hasRotation) { node.has_rotation = true;
            node.rotation[0] = src.rotation[0]; node.rotation[1] = src.rotation[1]; node.rotation[2] = src.rotation[2]; node.rotation[3] = src.rotation[3]; }
        if (src.hasScale) { node.has_scale = true;
            node.scale[0] = src.scale[0]; node.scale[1] = src.scale[1]; node.scale[2] = src.scale[2]; }

        if (src.parentIndex < 0)
            rootNodeIndices.push_back(static_cast<int>(hi));
    }

    // ---- Fill ----
    cgltf_buffer &buf = flatBuffers_[0];
    buf = {};
    buf.size = bufferData_.size();
    ownedStrings_.push_back(MallocString("exported_buffer"));
    buf.name = ownedStrings_.back();
    if (isGlb)
    {
        // GLB: buffer embedded in binary chunk — set data pointer, no URI
        buf.data = bufferData_.data();
        buf.data_free_method = cgltf_data_free_method_none;
        buf.uri = nullptr;
    }
    else
    {
        // glTF: buffer as external .bin file — set URI only
        ownedStrings_.push_back(MallocString(binFilename));
        buf.uri = ownedStrings_.back();
        buf.data_free_method = cgltf_data_free_method_none;
    }

    for (size_t textureIndex = 0; textureIndex < uniqueTextureUris.size(); ++textureIndex)
    {
        cgltf_image &image = flatImages_[textureIndex];
        image = {};
        ownedStrings_.push_back(MallocString(uniqueTextureUris[textureIndex]));
        image.uri = ownedStrings_.back();

        cgltf_texture &texture = flatTextures_[textureIndex];
        texture = {};
        texture.image = &image;
    }

    size_t bvIdx = 0;
    size_t accIdx = 0;
    size_t attrIdx = 0;
    size_t bufOffset = 0;
    size_t nextFlatMeshIdx = 0;

    for (size_t m = 0; m < numMeshes; ++m)
    {
        const auto &mesh = meshes_[m];

        // ---- Position buffer view + accessor ----
        bufOffset = Align4(bufOffset);
        cgltf_buffer_view &posBV = flatBufferViews_[bvIdx++];
        posBV = {};
        posBV.buffer = &buf;
        posBV.offset = bufOffset;
        posBV.size = mesh.positions.size() * sizeof(float);
        posBV.stride = sizeof(float) * 3;
        posBV.type = cgltf_buffer_view_type_vertices;

        cgltf_accessor &posAcc = flatAccessors_[accIdx++];
        posAcc = {};
        posAcc.buffer_view = &posBV;
        posAcc.count = mesh.positions.size() / 3;
        posAcc.type = cgltf_type_vec3;
        posAcc.component_type = cgltf_component_type_r_32f;
        posAcc.stride = sizeof(float) * 3;
        ownedStrings_.push_back(MallocString(mesh.name + "_positions"));
        posAcc.name = ownedStrings_.back();
        bufOffset += posBV.size;

        // ---- Normal buffer view + accessor ----
        cgltf_accessor *nrmAcc = nullptr;
        if (mesh.hasNormals)
        {
            bufOffset = Align4(bufOffset);
            cgltf_buffer_view &nrmBV = flatBufferViews_[bvIdx++];
            nrmBV = {};
            nrmBV.buffer = &buf;
            nrmBV.offset = bufOffset;
            nrmBV.size = mesh.normals.size() * sizeof(float);
            nrmBV.stride = sizeof(float) * 3;
            nrmBV.type = cgltf_buffer_view_type_vertices;

            cgltf_accessor &nrmAccRef = flatAccessors_[accIdx++];
            nrmAccRef = {};
            nrmAccRef.buffer_view = &nrmBV;
            nrmAccRef.count = mesh.normals.size() / 3;
            nrmAccRef.type = cgltf_type_vec3;
            nrmAccRef.component_type = cgltf_component_type_r_32f;
            nrmAccRef.stride = sizeof(float) * 3;
            ownedStrings_.push_back(MallocString(mesh.name + "_normals"));
            nrmAccRef.name = ownedStrings_.back();
            nrmAcc = &nrmAccRef;
            bufOffset += nrmBV.size;
        }

        // ---- UV buffer views + accessors (TEXCOORD_0..N) ----
        struct UvAccInfo { cgltf_accessor *acc; int setIndex; };
        std::vector<UvAccInfo> uvAccs;
        for (size_t u = 0; u < mesh.uvSets.size(); ++u)
        {
            bufOffset = Align4(bufOffset);
            cgltf_buffer_view &uvBV = flatBufferViews_[bvIdx++];
            uvBV = {};
            uvBV.buffer = &buf;
            uvBV.offset = bufOffset;
            uvBV.size = mesh.uvSets[u].data.size() * sizeof(float);
            uvBV.stride = sizeof(float) * 2;
            uvBV.type = cgltf_buffer_view_type_vertices;

            cgltf_accessor &uvAccRef = flatAccessors_[accIdx++];
            uvAccRef = {};
            uvAccRef.buffer_view = &uvBV;
            uvAccRef.count = mesh.uvSets[u].data.size() / 2;
            uvAccRef.type = cgltf_type_vec2;
            uvAccRef.component_type = cgltf_component_type_r_32f;
            uvAccRef.stride = sizeof(float) * 2;
            ownedStrings_.push_back(MallocString(mesh.name + "_uv" + std::to_string(u)));
            uvAccRef.name = ownedStrings_.back();
            uvAccs.push_back({&uvAccRef, static_cast<int>(u)});
            bufOffset += uvBV.size;
        }

        // ---- Vertex color buffer view + accessor (COLOR_0) ----
        cgltf_accessor *colorAcc = nullptr;
        if (mesh.hasColors)
        {
            bufOffset = Align4(bufOffset);
            const size_t colorComp = (mesh.colorComponents == 3) ? 3 : 4;
            const size_t colorFloatCount = mesh.colors.size();
            cgltf_buffer_view &colorBV = flatBufferViews_[bvIdx++];
            colorBV = {};
            colorBV.buffer = &buf;
            colorBV.offset = bufOffset;
            colorBV.size = colorFloatCount * sizeof(float);
            colorBV.stride = static_cast<cgltf_size>(colorComp) * sizeof(float);
            colorBV.type = cgltf_buffer_view_type_vertices;

            cgltf_accessor &colorAccRef = flatAccessors_[accIdx++];
            colorAccRef = {};
            colorAccRef.buffer_view = &colorBV;
            colorAccRef.count = colorFloatCount / colorComp;
            colorAccRef.type = (colorComp == 3) ? cgltf_type_vec3 : cgltf_type_vec4;
            colorAccRef.component_type = cgltf_component_type_r_32f;
            colorAccRef.stride = static_cast<cgltf_size>(colorComp) * sizeof(float);
            ownedStrings_.push_back(MallocString(mesh.name + "_colors"));
            colorAccRef.name = ownedStrings_.back();
            colorAcc = &colorAccRef;
            bufOffset += colorBV.size;
        }

        // ---- Index buffer view + accessor ----
        bufOffset = Align4(bufOffset);
        cgltf_buffer_view &idxBV = flatBufferViews_[bvIdx++];
        idxBV = {};
        idxBV.buffer = &buf;
        idxBV.offset = bufOffset;
        idxBV.size = mesh.indices.size() * sizeof(uint32_t);
        idxBV.stride = sizeof(uint32_t);
        idxBV.type = cgltf_buffer_view_type_indices;

        cgltf_accessor &idxAcc = flatAccessors_[accIdx++];
        idxAcc = {};
        idxAcc.buffer_view = &idxBV;
        idxAcc.count = mesh.indices.size();
        idxAcc.type = cgltf_type_scalar;
        idxAcc.component_type = cgltf_component_type_r_32u;
        idxAcc.stride = sizeof(uint32_t);
        ownedStrings_.push_back(MallocString(mesh.name + "_indices"));
        idxAcc.name = ownedStrings_.back();
        bufOffset += idxBV.size;

        // ---- Skin JOINT_0 / WEIGHTS_0 buffer views + accessors ----
        cgltf_accessor *jointsAcc = nullptr;
        cgltf_accessor *weightsAcc = nullptr;
        if (mesh.hasSkin)
        {
            // JOINTS_0 buffer view + accessor (VEC4, uint32 per component)
            bufOffset = Align4(bufOffset);
            cgltf_buffer_view &jiBV = flatBufferViews_[bvIdx++];
            jiBV = {};
            jiBV.buffer = &buf;
            jiBV.offset = bufOffset;
            jiBV.size = mesh.jointIndices.size() * sizeof(uint16_t);
            jiBV.stride = sizeof(uint16_t) * 4;
            jiBV.type = cgltf_buffer_view_type_vertices;

            cgltf_accessor &jiAcc = flatAccessors_[accIdx++];
            jiAcc = {};
            jiAcc.buffer_view = &jiBV;
            jiAcc.count = mesh.jointIndices.size() / 4;
            jiAcc.type = cgltf_type_vec4;
            jiAcc.component_type = cgltf_component_type_r_16u;
            jiAcc.stride = sizeof(uint16_t) * 4;
            ownedStrings_.push_back(MallocString(mesh.name + "_joints"));
            jiAcc.name = ownedStrings_.back();
            jointsAcc = &jiAcc;
            bufOffset += jiBV.size;

            // WEIGHTS_0 buffer view + accessor (VEC4, float per component)
            bufOffset = Align4(bufOffset);
            cgltf_buffer_view &jwBV = flatBufferViews_[bvIdx++];
            jwBV = {};
            jwBV.buffer = &buf;
            jwBV.offset = bufOffset;
            jwBV.size = mesh.jointWeights.size() * sizeof(float);
            jwBV.stride = sizeof(float) * 4;
            jwBV.type = cgltf_buffer_view_type_vertices;

            cgltf_accessor &jwAcc = flatAccessors_[accIdx++];
            jwAcc = {};
            jwAcc.buffer_view = &jwBV;
            jwAcc.count = mesh.jointWeights.size() / 4;
            jwAcc.type = cgltf_type_vec4;
            jwAcc.component_type = cgltf_component_type_r_32f;
            jwAcc.stride = sizeof(float) * 4;
            ownedStrings_.push_back(MallocString(mesh.name + "_weights"));
            jwAcc.name = ownedStrings_.back();
            weightsAcc = &jwAcc;
            bufOffset += jwBV.size;
        }

        // ---- Attributes ----
        const size_t firstAttrIndex = attrIdx;

        cgltf_attribute *posAttr = &flatAttributes_[attrIdx];
        ownedStrings_.push_back(MallocString("POSITION"));
        posAttr->name = ownedStrings_.back();
        posAttr->type = cgltf_attribute_type_position;
        posAttr->data = &posAcc;
        attrIdx++;

        if (mesh.hasNormals && nrmAcc)
        {
            cgltf_attribute *nrmAttr = &flatAttributes_[attrIdx];
            ownedStrings_.push_back(MallocString("NORMAL"));
            nrmAttr->name = ownedStrings_.back();
            nrmAttr->type = cgltf_attribute_type_normal;
            nrmAttr->data = nrmAcc;
            attrIdx++;
        }

        // TEXCOORD_0..N
        for (size_t u = 0; u < uvAccs.size(); ++u)
        {
            cgltf_attribute *uvAttr = &flatAttributes_[attrIdx];
            const std::string texCoordName = "TEXCOORD_" + std::to_string(uvAccs[u].setIndex);
            ownedStrings_.push_back(MallocString(texCoordName));
            uvAttr->name = ownedStrings_.back();
            uvAttr->type = cgltf_attribute_type_texcoord;
            uvAttr->index = uvAccs[u].setIndex;
            uvAttr->data = uvAccs[u].acc;
            attrIdx++;
        }

        // COLOR_0
        if (mesh.hasColors && colorAcc)
        {
            cgltf_attribute *colorAttr = &flatAttributes_[attrIdx];
            ownedStrings_.push_back(MallocString("COLOR_0"));
            colorAttr->name = ownedStrings_.back();
            colorAttr->type = cgltf_attribute_type_color;
            colorAttr->index = 0;
            colorAttr->data = colorAcc;
            attrIdx++;
        }

        // JOINTS_0 + WEIGHTS_0 (skinning)
        if (mesh.hasSkin && jointsAcc && weightsAcc)
        {
            cgltf_attribute *jointsAttr = &flatAttributes_[attrIdx];
            ownedStrings_.push_back(MallocString("JOINTS_0"));
            jointsAttr->name = ownedStrings_.back();
            jointsAttr->type = cgltf_attribute_type_joints;
            jointsAttr->index = 0;
            jointsAttr->data = jointsAcc;
            attrIdx++;

            cgltf_attribute *weightsAttr = &flatAttributes_[attrIdx];
            ownedStrings_.push_back(MallocString("WEIGHTS_0"));
            weightsAttr->name = ownedStrings_.back();
            weightsAttr->type = cgltf_attribute_type_weights;
            weightsAttr->index = 0;
            weightsAttr->data = weightsAcc;
            attrIdx++;
        }

        // ---- Primitive ----
        cgltf_primitive &prim = flatPrimitives_[m];
        prim = {};
        prim.type = cgltf_primitive_type_triangles;
        prim.indices = &idxAcc;
        prim.attributes = &flatAttributes_[firstAttrIndex];
        prim.attributes_count = attrIdx - firstAttrIndex;

        // ---- Mesh ----
        cgltf_mesh &cmesh = flatMeshes_[m];
        cmesh = {};
        ownedStrings_.push_back(MallocString(mesh.name));
        cmesh.name = ownedStrings_.back();
        cmesh.primitives = &prim;
        cmesh.primitives_count = 1;

        // ---- Material ----
        if (mesh.material.hasPBR)
        {
            cgltf_material &mat = flatMaterials_[m];
            mat = {};
            ownedStrings_.push_back(MallocString(mesh.material.name));
            mat.name = ownedStrings_.back();

            auto assignTextureView = [&](cgltf_texture_view &textureView, const std::string &texturePath)
            {
                const std::string uri = normalizeTextureUri(texturePath);
                const auto textureIt = textureIndexByUri.find(uri);
                if (textureIt == textureIndexByUri.end())
                {
                    return;
                }

                textureView.texture = &flatTextures_[textureIt->second];
            };

            // PBR metallic-roughness
            mat.has_pbr_metallic_roughness = true;
            mat.pbr_metallic_roughness.base_color_factor[0] = mesh.material.baseColorFactor[0];
            mat.pbr_metallic_roughness.base_color_factor[1] = mesh.material.baseColorFactor[1];
            mat.pbr_metallic_roughness.base_color_factor[2] = mesh.material.baseColorFactor[2];
            mat.pbr_metallic_roughness.base_color_factor[3] = mesh.material.baseColorFactor[3];
            mat.pbr_metallic_roughness.metallic_factor = mesh.material.metallicFactor;
            mat.pbr_metallic_roughness.roughness_factor = mesh.material.roughnessFactor;
            assignTextureView(mat.pbr_metallic_roughness.base_color_texture, mesh.material.baseColorTexturePath);
            assignTextureView(mat.pbr_metallic_roughness.metallic_roughness_texture, mesh.material.metallicRoughnessTexturePath);

            // Emissive
            mat.emissive_factor[0] = mesh.material.emissiveFactor[0];
            mat.emissive_factor[1] = mesh.material.emissiveFactor[1];
            mat.emissive_factor[2] = mesh.material.emissiveFactor[2];
            assignTextureView(mat.emissive_texture, mesh.material.emissiveTexturePath);
            assignTextureView(mat.normal_texture, mesh.material.normalTexturePath);
            assignTextureView(mat.occlusion_texture, mesh.material.occlusionTexturePath);
            if (mat.normal_texture.texture)
            {
                mat.normal_texture.scale = 1.0f;
            }
            if (mat.occlusion_texture.texture)
            {
                mat.occlusion_texture.scale = 1.0f;
            }

            mat.alpha_mode = mesh.material.alphaMode;
            mat.alpha_cutoff = mesh.material.alphaCutoff;
            mat.double_sided = mesh.material.doubleSided ? 1 : 0;

            if (mesh.material.specularFactor != 1.0f ||
                mesh.material.specularColorFactor[0] != 1.0f ||
                mesh.material.specularColorFactor[1] != 1.0f ||
                mesh.material.specularColorFactor[2] != 1.0f ||
                !mesh.material.specularTexturePath.empty() ||
                !mesh.material.specularColorTexturePath.empty())
            {
                mat.has_specular = true;
                mat.specular.specular_factor = mesh.material.specularFactor;
                mat.specular.specular_color_factor[0] = mesh.material.specularColorFactor[0];
                mat.specular.specular_color_factor[1] = mesh.material.specularColorFactor[1];
                mat.specular.specular_color_factor[2] = mesh.material.specularColorFactor[2];
                assignTextureView(mat.specular.specular_texture, mesh.material.specularTexturePath);
                assignTextureView(mat.specular.specular_color_texture, mesh.material.specularColorTexturePath);
            }

            if (mesh.material.ior != 1.5f)
            {
                mat.has_ior = true;
                mat.ior.ior = mesh.material.ior;
            }

            if (mesh.material.transmissionFactor > 0.0f || !mesh.material.transmissionTexturePath.empty())
            {
                mat.has_transmission = true;
                mat.transmission.transmission_factor = mesh.material.transmissionFactor;
                assignTextureView(mat.transmission.transmission_texture, mesh.material.transmissionTexturePath);
            }

            if (mesh.material.clearcoatFactor > 0.0f ||
                mesh.material.clearcoatRoughnessFactor > 0.0f ||
                !mesh.material.clearcoatTexturePath.empty() ||
                !mesh.material.clearcoatRoughnessTexturePath.empty() ||
                !mesh.material.clearcoatNormalTexturePath.empty())
            {
                mat.has_clearcoat = true;
                mat.clearcoat.clearcoat_factor = mesh.material.clearcoatFactor;
                mat.clearcoat.clearcoat_roughness_factor = mesh.material.clearcoatRoughnessFactor;
                assignTextureView(mat.clearcoat.clearcoat_texture, mesh.material.clearcoatTexturePath);
                assignTextureView(mat.clearcoat.clearcoat_roughness_texture, mesh.material.clearcoatRoughnessTexturePath);
                assignTextureView(mat.clearcoat.clearcoat_normal_texture, mesh.material.clearcoatNormalTexturePath);
            }

            if (HasAnyNonZeroColor(mesh.material.sheenColorFactor) ||
                mesh.material.sheenRoughnessFactor > 0.0f ||
                !mesh.material.sheenColorTexturePath.empty() ||
                !mesh.material.sheenRoughnessTexturePath.empty())
            {
                mat.has_sheen = true;
                mat.sheen.sheen_color_factor[0] = mesh.material.sheenColorFactor[0];
                mat.sheen.sheen_color_factor[1] = mesh.material.sheenColorFactor[1];
                mat.sheen.sheen_color_factor[2] = mesh.material.sheenColorFactor[2];
                mat.sheen.sheen_roughness_factor = mesh.material.sheenRoughnessFactor;
                assignTextureView(mat.sheen.sheen_color_texture, mesh.material.sheenColorTexturePath);
                assignTextureView(mat.sheen.sheen_roughness_texture, mesh.material.sheenRoughnessTexturePath);
            }

            prim.material = &mat;
        }

        // Find if this mesh has a hierarchy node parent (non-root)
        bool attachedToHierarchy = false;
        for (auto &en : exportedNodes_)
        {
            if (en.meshIndex == static_cast<int>(m) && en.gltfNodeIndex >= 0)
            {
                cgltf_node &hnode = flatNodes_[en.gltfNodeIndex];
                hnode.mesh = &cmesh;
                attachedToHierarchy = true;
                break;
            }
        }

        if (!attachedToHierarchy)
        {
            int meshNodeIdx = static_cast<int>(hierNodesToCreate + nextFlatMeshIdx);
            cgltf_node &node = flatNodes_[meshNodeIdx];
            node = {};
            ownedStrings_.push_back(MallocString(mesh.name));
            node.name = ownedStrings_.back();
            node.mesh = &cmesh;
            flatSceneNodes_[nextFlatMeshIdx] = &node;
            nextFlatMeshIdx++;
        }
    }

    // ---- Skin entries (IBM buffer views + accessors + cgltf_skin) ----
    // IBM accessors/buffer views go into the MAIN flat arrays so cgltf can find them.
    {
        size_t skinIdx = 0;
        // IBM data starts after all mesh geometry + joint data
        size_t ibmBufOffset = Align4(bufOffset);

        for (size_t m = 0; m < numMeshes; ++m)
        {
            const auto &mesh = meshes_[m];
            if (!mesh.hasSkin) continue;

            const size_t numJoints = mesh.joints.size();
            if (numJoints == 0) continue;

            // ---- IBM buffer view (in main array) ----
            const size_t ibmSize = numJoints * 16 * sizeof(float);
            ibmBufOffset = Align4(ibmBufOffset);

            cgltf_buffer_view &ibmBV = flatBufferViews_[bvIdx++];
            ibmBV = {};
            ibmBV.buffer = &buf;
            ibmBV.offset = ibmBufOffset;
            ibmBV.size = ibmSize;
            ibmBV.stride = sizeof(float) * 16;
            ibmBV.type = cgltf_buffer_view_type_vertices;

            // ---- IBM accessor (in main array) ----
            cgltf_accessor &ibmAcc = flatAccessors_[accIdx++];
            ibmAcc = {};
            ibmAcc.buffer_view = &ibmBV;
            ibmAcc.count = numJoints;
            ibmAcc.type = cgltf_type_mat4;
            ibmAcc.component_type = cgltf_component_type_r_32f;
            ibmAcc.stride = sizeof(float) * 16;
            ownedStrings_.push_back(MallocString(mesh.name + "_ibm"));
            ibmAcc.name = ownedStrings_.back();

            // ---- Build joint node pointer array (use pre-allocated slot) ----
            if (skinIdx >= flatSkinJointArrays_.size()) break;
            auto &jointPtrs = flatSkinJointArrays_[skinIdx];
            jointPtrs.clear();
            jointPtrs.reserve(numJoints);
            for (const auto &joint : mesh.joints)
            {
                auto it = dagPathToNodeIndex_.find(joint.dagPath);
                if (it != dagPathToNodeIndex_.end() && exportedNodes_[it->second].gltfNodeIndex >= 0)
                {
                    const int ji = exportedNodes_[it->second].gltfNodeIndex;
                    if (ji >= 0 && static_cast<size_t>(ji) < flatNodes_.size())
                        jointPtrs.push_back(&flatNodes_[ji]);
                }
            }

            // ---- cgltf_skin ----
            cgltf_skin &cskin = flatSkins_[skinIdx];
            cskin = {};
            ownedStrings_.push_back(MallocString(mesh.name + "_skin"));
            cskin.name = ownedStrings_.back();
            cskin.inverse_bind_matrices = &ibmAcc;
            cskin.joints = jointPtrs.data();
            cskin.joints_count = jointPtrs.size();

            // ---- Set node.skin for the mesh's direct parent node ----
            // Find which flat node holds this mesh, then assign skin
            for (size_t fi = 0; fi < flatNodes_.size(); ++fi)
            {
                if (flatNodes_[fi].mesh == &flatMeshes_[m])
                {
                    flatNodes_[fi].skin = &flatSkins_[skinIdx];
                    break;
                }
            }

            ibmBufOffset += ibmSize;
            skinIdx++;
        }
    }

    // ---- Cameras ----
    flatCameras_.resize(cameras_.size());
    for (size_t c = 0; c < cameras_.size(); ++c)
    {
        const auto &srcCam = cameras_[c];
        cgltf_camera &cam = flatCameras_[c];
        cam = {};
        ownedStrings_.push_back(MallocString(srcCam.name));
        cam.name = ownedStrings_.back();

        // Find the hierarchy node this camera belongs to
        auto it = dagPathToNodeIndex_.find(srcCam.nodeDagPath);
        if (it != dagPathToNodeIndex_.end() && exportedNodes_[it->second].gltfNodeIndex >= 0)
        {
            int ni = exportedNodes_[it->second].gltfNodeIndex;
            flatNodes_[ni].camera = &cam;
        }

        if (srcCam.isPerspective)
        {
            cam.type = cgltf_camera_type_perspective;
            cam.data.perspective.aspect_ratio = srcCam.aspectRatio;
            cam.data.perspective.yfov = srcCam.yfov;
            cam.data.perspective.has_aspect_ratio = (srcCam.aspectRatio > 0.0f);
            cam.data.perspective.znear = srcCam.znear;
            cam.data.perspective.zfar = srcCam.zfar;
            cam.data.perspective.has_zfar = (srcCam.zfar > 0.0f && srcCam.zfar < 1.0e10f);
        }
        else
        {
            cam.type = cgltf_camera_type_orthographic;
            cam.data.orthographic.xmag = srcCam.xmag;
            cam.data.orthographic.ymag = srcCam.ymag;
            cam.data.orthographic.znear = srcCam.znear;
            cam.data.orthographic.zfar = srcCam.zfar;
        }
    }

    // ---- Animation entries ----
    if (!sampledAnimations_.empty())
    {
        // Compute where animation data starts in bufferData_
        // After all mesh geometry + joint data (bufOffset) + IBM data
        size_t animBufOffset = Align4(bufOffset);
        for (const auto &mesh : meshes_)
            if (mesh.hasSkin)
                animBufOffset = Align4(animBufOffset + mesh.joints.size() * 16 * sizeof(float));

        for (size_t a = 0; a < sampledAnimations_.size(); ++a)
        {
            auto &sa = sampledAnimations_[a];

            // Set target_node for each channel
            for (auto &chan : sa.channels)
            {
                if (sa.targetNodeGltfIndex >= 0 &&
                    static_cast<size_t>(sa.targetNodeGltfIndex) < flatNodes_.size())
                {
                    chan.target_node = &flatNodes_[sa.targetNodeGltfIndex];
                }
            }

            // Set buffer and offset for all buffer views
            for (size_t i = 0; i < sa.inputBufferViews.size(); ++i)
            {
                animBufOffset = Align4(animBufOffset);
                sa.inputBufferViews[i].buffer = &buf;
                sa.inputBufferViews[i].offset = animBufOffset;
                animBufOffset += sa.inputBufferViews[i].size;
            }
            for (size_t i = 0; i < sa.outputBufferViews.size(); ++i)
            {
                animBufOffset = Align4(animBufOffset);
                sa.outputBufferViews[i].buffer = &buf;
                sa.outputBufferViews[i].offset = animBufOffset;
                animBufOffset += sa.outputBufferViews[i].size;
            }

            // Fix accessor buffer_view pointers
            for (size_t i = 0; i < sa.inputAccessors.size(); ++i)
                if (i < sa.inputBufferViews.size())
                    sa.inputAccessors[i].buffer_view = &sa.inputBufferViews[i];
            for (size_t i = 0; i < sa.outputAccessors.size(); ++i)
                if (i < sa.outputBufferViews.size())
                    sa.outputAccessors[i].buffer_view = &sa.outputBufferViews[i];

            // Copy into flat animation array
            flatAnimations_[a] = sa.glAnim;
        }
    }

    // ---- Build children arrays for hierarchy nodes ----
    std::map<int, std::vector<int>> parentToChildren;
    for (auto &en : exportedNodes_) {
        int cIdx = en.gltfNodeIndex;
        if (cIdx < 0) continue;
        if (en.parentIndex >= 0 && exportedNodes_[en.parentIndex].gltfNodeIndex >= 0)
            parentToChildren[exportedNodes_[en.parentIndex].gltfNodeIndex].push_back(cIdx);
        else
            rootNodeIndices.push_back(cIdx);
    }
    for (auto &pc : parentToChildren) {
        auto &children = flatNodeChildren_.emplace_back();
        for (int ci : pc.second) children.push_back(&flatNodes_[ci]);
        flatNodes_[pc.first].children = children.data();
        flatNodes_[pc.first].children_count = children.size();
    }
    flatSceneNodes_.clear();
    flatSceneNodes_.reserve(rootNodeIndices.size() + flatMeshNodes);
    for (int rootIdx : rootNodeIndices)
        flatSceneNodes_.push_back(&flatNodes_[rootIdx]);
    for (size_t f = 0; f < flatMeshNodes; ++f)
        flatSceneNodes_.push_back(&flatNodes_[hierNodesToCreate + f]);

    // ---- Scene ----
    cgltf_scene scene = {};
    ownedStrings_.push_back(MallocString("Root Scene"));
    scene.name = ownedStrings_.back();
    scene.nodes = flatSceneNodes_.data();
    scene.nodes_count = flatSceneNodes_.size();
    flatScenes_.push_back(scene);

    // ---- cgltf_data ----
    document_ = (cgltf_data *)std::calloc(1, sizeof(cgltf_data));
    if (!document_)
    {
        return maya_gltf::ReportError("maya_gltf: out of memory allocating cgltf_data");
    }

    document_->file_type = cgltf_file_type_gltf;
    ownedStrings_.push_back(MallocString("2.0"));
    document_->asset.version = ownedStrings_.back();
    ownedStrings_.push_back(MallocString("MayaCustomFormatLib (cgltf)"));
    document_->asset.generator = ownedStrings_.back();

    assignDocumentPointers();

    return MS::kSuccess;
}

void GltfSceneExporter::assignDocumentPointers()
{
    if (!document_) return;

    document_->buffers_count = flatBuffers_.size();
    document_->buffers = flatBuffers_.data();

    document_->buffer_views_count = flatBufferViews_.size();
    document_->buffer_views = flatBufferViews_.data();

    document_->accessors_count = flatAccessors_.size();
    document_->accessors = flatAccessors_.data();

    document_->materials_count = flatMaterials_.size();
    document_->materials = flatMaterials_.data();

    document_->images_count = flatImages_.size();
    document_->images = flatImages_.data();

    document_->textures_count = flatTextures_.size();
    document_->textures = flatTextures_.data();

    document_->meshes_count = flatMeshes_.size();
    document_->meshes = flatMeshes_.data();

    document_->cameras_count = flatCameras_.size();
    document_->cameras = flatCameras_.data();

    document_->nodes_count = flatNodes_.size();
    document_->nodes = flatNodes_.data();

    document_->skins_count = flatSkins_.size();
    document_->skins = flatSkins_.data();

    document_->animations_count = flatAnimations_.size();
    document_->animations = flatAnimations_.data();

    document_->scenes_count = flatScenes_.size();
    document_->scenes = flatScenes_.data();
    document_->scene = &flatScenes_[0];
}

void GltfSceneExporter::safeFree()
{
    for (char *s : ownedStrings_) std::free(s);
    ownedStrings_.clear();

    std::free(document_);
    document_ = nullptr;

    flatBuffers_.clear();
    flatBufferViews_.clear();
    flatAccessors_.clear();
    flatMeshes_.clear();
    flatPrimitives_.clear();
    flatAttributes_.clear();
    flatMaterials_.clear();
    flatTextures_.clear();
    flatImages_.clear();
    flatNodes_.clear();
    flatScenes_.clear();
    flatSceneNodes_.clear();
    flatNodeChildren_.clear();
    flatCameras_.clear();
    flatSkins_.clear();
    flatSkinJointArrays_.clear();
    flatAnimations_.clear();
    flatAnimChannels_.clear();
    flatAnimSamplers_.clear();
    sampledAnimations_.clear();
}

std::string GltfSceneExporter::getBasename(const std::string &path)
{
    const auto extPos = path.rfind('.');
    if (extPos != std::string::npos)
    {
        return path.substr(0, extPos);
    }
    return path;
}
