#include "GltfSceneImporter.h"

#include <common_gltf/MayaGltfCommon.h>
#include <common_gltf/SimpleGltfDocument.h>
#include <common_gltf/cgltf.h>
#include "GltfAnimationImporter.h"
#include "GltfMaterialImporter.h"
#include "GltfMeshBuilder.h"
#include <common/MayaCommandUtils.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MColorArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDGModifier.h>
#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MAngle.h>
#include <maya/MEulerRotation.h>
#include <maya/MQuaternion.h>
#include <maya/MDagPathArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>



static MMatrix ConvertGltfMatrixToMayaMatrix(const float *m)
{
    double mat[4][4] = {
        { m[0],  m[1],  m[2],  m[3] },
        { m[4],  m[5],  m[6],  m[7] },
        { m[8],  m[9],  m[10], m[11] },
        { m[12], m[13], m[14], m[15] }
    };
    return MMatrix(mat);
}

static MStatus ApplyMatrixAsTransform(MFnTransform &transformFn, const MMatrix &matrix)
{
    MStatus status;
    MTransformationMatrix transform(matrix);

    const MVector translation = transform.getTranslation(MSpace::kTransform, &status);
    if (!status)
    {
        return status;
    }

    MQuaternion rotation = transform.rotation();
    double scale[3] = {1.0, 1.0, 1.0};
    transform.getScale(scale, MSpace::kTransform);

    status = transformFn.setTranslation(translation, MSpace::kTransform);
    if (!status)
    {
        return status;
    }

    status = transformFn.setRotation(rotation);
    if (!status)
    {
        return status;
    }

    status = transformFn.setScale(scale);
    return status ? MS::kSuccess : MS::kFailure;
}




// ============================================================================

GltfSceneImporter::GltfSceneImporter(
    std::shared_ptr<const simple_gltf::Document> document,
    const GltfImportOptions &importOptions)
    : document_(std::move(document))
    , importOptions_(importOptions)
{
}

MStatus GltfSceneImporter::Import()
{
    const cgltf_data *data = document_->GetData();
    if (!data)
    {
        return maya_gltf::ReportError("maya_gltf: no glTF data to import");
    }

    // Enable Maya color management up-front so per-texture colorSpace settings
    // on imported file nodes actually take effect.
    if (importOptions_.enableColorManagement)
    {
        if (!maya_cmd::EnsureColorManagementEnabled())
        {
            maya_gltf::ReportWarning("maya_gltf: failed to enable color management for import");
        }
    }

    // Create import root if needed
    if (!importOptions_.useSceneRoot)
    {
        MStatus status;
        MFnTransform rootTransformFn;
        importRoot_ = rootTransformFn.create(MObject::kNullObj, &status);
        if (!status)
        {
            return maya_gltf::ReportError("maya_gltf: failed to create import root", status);
        }
        rootTransformFn.setName("gltf_import_root#");
    }

    // Build joint set (which cgltf nodes are referenced as joints by any skin)
    if (importOptions_.importSkin)
    {
        buildJointSet(data);
    }

    // If there's a default scene, use it; otherwise import all top-level nodes
    MStatus status = MS::kSuccess;
    if (data->scene)
    {
        status = importScene(data);
    }
    else if (data->scenes_count > 0)
    {
        // Import all scenes
        for (cgltf_size i = 0; i < data->scenes_count; ++i)
        {
            const_cast<cgltf_data *>(data)->scene = &data->scenes[i];
            status = importScene(data);
            const_cast<cgltf_data *>(data)->scene = nullptr;
            if (!status) break;
        }
    }
    else
    {
        // Scene-less document: import only root nodes. cgltf already resolves
        // parent pointers, so importing all nodes here would duplicate
        // hierarchy branches and break joint lookup.
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
        {
            if (data->nodes[i].parent != nullptr)
            {
                continue;
            }
            const MObject parent = importRoot_.isNull() ? MObject::kNullObj : importRoot_;
            status = createNodeHierarchy(&data->nodes[i], parent);
            if (!status) break;
        }
    }

    if (!status) return status;

    // Delegate skinning and animation to injected sub-modules.
    // GltfSkinClusterBuilder receives importedNodePaths_ via constructor injection.
    // GltfAnimationImporter receives document_, importedNodePaths_, importOptions_
    // via constructor injection — no coupling to GltfSceneImporter state.
    {
        GltfSkinClusterBuilder skinBuilder(importedNodePaths_);
        GltfAnimationImporter animImporter(document_, importedNodePaths_, importOptions_);
        GltfMaterialImporter materialImporter(document_);

    if (importOptions_.restoreDefaultPoseAfterSkin)
    {
        if (importOptions_.importSkin && !pendingSkins_.empty())
        {
            status = skinBuilder.SetupSkins(data, pendingSkins_);
            if (!status)
            {
                maya_gltf::ReportWarning("maya_gltf: skin setup completed with errors");
            }
        }

        if (importOptions_.importAnimation)
        {
            status = animImporter.ImportAnimations(data);
            if (!status)
            {
                maya_gltf::ReportWarning("maya_gltf: animation import completed with errors");
            }
        }
    }
    else
    {
        if (importOptions_.importAnimation)
        {
            status = animImporter.ImportAnimations(data);
            if (!status)
            {
                maya_gltf::ReportWarning("maya_gltf: animation import completed with errors");
            }
        }

        if (importOptions_.importSkin && !pendingSkins_.empty())
        {
            // Bind pose = first animation frame. Set the current time to the
            // first frame so the skeleton is at that pose when setupSkins
            // records the bind pose, then restore the user's current time.
            // glTF animations start at time 0; animCurve pre-infinity is
            // constant, so time 0 evaluates to the first keyframe value.
            const MTime savedTime = MAnimControl::currentTime();
            if (importOptions_.importAnimation)
            {
                MAnimControl::setCurrentTime(MTime(0.0, MTime::uiUnit()));
            }
            status = skinBuilder.SetupSkins(data, pendingSkins_);
            if (!status)
            {
                maya_gltf::ReportWarning("maya_gltf: skin setup completed with errors");
            }
            if (importOptions_.importAnimation)
            {
                MAnimControl::setCurrentTime(savedTime);
            }
        }
    }

    }

    return MS::kSuccess;
}

MStatus GltfSceneImporter::importScene(const cgltf_data *data)
{
    if (!data->scene) return MS::kSuccess;

    const cgltf_scene *scene = data->scene;
    const MObject parent = importRoot_.isNull() ? MObject::kNullObj : importRoot_;

    for (cgltf_size i = 0; i < scene->nodes_count; ++i)
    {
        MStatus status = createNodeHierarchy(scene->nodes[i], parent);
        if (!status) return status;
    }

    return MS::kSuccess;
}

MStatus GltfSceneImporter::createNodeHierarchy(const cgltf_node *node, MObject parentTransform)
{
    if (!node) return MS::kSuccess;

    MStatus status;
    MString nodeName = sanitizeNodeName(node->name ? node->name : "gltf_node");

    // Create as joint or transform depending on whether this node is a skeleton joint
    MObject transformObj;
    if (isJoint(node))
    {
        transformObj = createNodeAsJoint(nodeName, parentTransform);
        if (transformObj.isNull())
        {
            return maya_gltf::ReportError(
                MString("maya_gltf: failed to create joint for node ") + node->name);
        }
    }
    else
    {
        MFnTransform transformFn;
        transformObj = transformFn.create(parentTransform, &status);
        if (!status)
        {
            return maya_gltf::ReportError(
                MString("maya_gltf: failed to create transform for node ") + node->name, status);
        }
        transformFn.setName(nodeName);
    }

    // Apply node transform
    status = applyNodeTransform(node, transformObj);
    if (!status) return status;

    if (isTopLevelNode(node) && !importOptions_.transformCorrection.IsIdentity())
    {
        status = dcc_import_transform::ApplyPreTransformToObject(
            transformObj,
            importOptions_.transformCorrection.Matrix());
        if (!status)
        {
            return maya_gltf::ReportError(
                MString("maya_gltf: failed to apply top-level correction to node ") + nodeName,
                status);
        }
    }

    if (isJoint(node))
    {
        MFnIkJoint jointFn(transformObj);
        MPlug sscPlug = jointFn.findPlug("segmentScaleCompensate", true, &status);
        if (status)
        {
            sscPlug.setBool(false);
        }
    }

    MDagPath transformPath;
    status = MDagPath::getAPathTo(transformObj, transformPath);
    if (status)
    {
        importedNodePaths_[node] = transformPath;
    }
    status = MS::kSuccess;

    // Import mesh if present — pass gltf node for skin tracking
    if (node->mesh && importOptions_.importMesh)
    {
        status = importMesh(node->mesh, nodeName.asChar(), transformObj, node);
        if (!status) return status;
    }

    // Import camera if present
    if (node->camera && importOptions_.importCamera)
    {
        status = importCamera(node->camera, transformObj);
        if (!status)
        {
            maya_gltf::ReportWarning(
                MString("maya_gltf: failed to import camera for node ") + node->name);
        }
    }

    // Recursively process children
    for (cgltf_size i = 0; i < node->children_count; ++i)
    {
        status = createNodeHierarchy(node->children[i], transformObj);
        if (!status) return status;
    }

    return MS::kSuccess;
}

MStatus GltfSceneImporter::applyNodeTransform(const cgltf_node *node, MObject &transformObj)
{
    if (!node)
    {
        return MS::kSuccess;
    }

    MFnTransform transformFn(transformObj);

    // For nodes with an explicit matrix property, decompose the 4x4 matrix into
    // translation / rotation / scale channels.  This matches the DMX/SMD importer
    // convention and preserves joint semantics more reliably for skinning.
    if (node->has_matrix)
    {
        cgltf_float localMatrix[16];
        cgltf_node_transform_local(node, localMatrix);
        const MMatrix mm = ConvertGltfMatrixToMayaMatrix(localMatrix);
        return ApplyMatrixAsTransform(transformFn, mm);
    }

    // For TRS nodes, set each component directly from the parsed glTF data
    // without going through a 4x4 matrix round-trip.  Avoiding the
    // MTransformationMatrix decomposition/recomposition eliminates cumulative
    // precision loss in deep joint hierarchies.
    MStatus status;

    if (node->has_translation)
    {
        status = transformFn.setTranslation(
            MVector(node->translation[0], node->translation[1], node->translation[2]),
            MSpace::kTransform);
        if (!status) return status;
    }

    if (node->has_rotation)
    {
        // glTF quaternion storage: [x, y, z, w]
        // Maya MQuaternion:         (x, y, z, w)
        const MQuaternion q(
            node->rotation[0],
            node->rotation[1],
            node->rotation[2],
            node->rotation[3]);
        status = transformFn.setRotation(q);
        if (!status) return status;
    }

    if (node->has_scale)
    {
        const double scale[3] = {
            static_cast<double>(node->scale[0]),
            static_cast<double>(node->scale[1]),
            static_cast<double>(node->scale[2])
        };
        status = transformFn.setScale(scale);
        if (!status) return status;
    }

    return MS::kSuccess;
}

bool GltfSceneImporter::isTopLevelNode(const cgltf_node *node) const
{
    return node && node->parent == nullptr;
}

MStatus GltfSceneImporter::importMesh(
    const cgltf_mesh *mesh,
    const std::string &nodeName,
    MObject parentTransform,
    const cgltf_node *gltfNode)
{
    const MString meshName = sanitizeNodeName(mesh->name ? mesh->name : nodeName);

    for (cgltf_size p = 0; p < mesh->primitives_count; ++p)
    {
        const cgltf_primitive &primitive = mesh->primitives[p];

        if (primitive.type != cgltf_primitive_type_triangles)
        {
            maya_gltf::ReportWarning(
                MString("maya_gltf: skipping non-triangle primitive type ") + static_cast<int>(primitive.type));
            continue;
        }

        GltfMeshData meshData;
        GltfMeshBuilder meshBuilder(primitive);
        if (!meshBuilder.Build(meshData))
        {
            maya_gltf::ReportWarning(
                MString("maya_gltf: failed to build mesh data for primitive ") + static_cast<int>(p)
                + " of mesh \"" + meshName + "\"");
            continue;
        }

        if (meshData.vertexArray.length() == 0 || meshData.polygonCounts.length() == 0)
        {
            continue;
        }

        // Create transform for this primitive
        MStatus status;
        MFnTransform primTransformFn;
        MObject primTransformObj = primTransformFn.create(parentTransform, &status);
        if (!status)
        {
            return maya_gltf::ReportError("maya_gltf: failed to create primitive transform", status);
        }

        MString primNodeName = meshName + (mesh->primitives_count > 1 ? MString("_") + static_cast<int>(p) : MString(""));
        primTransformFn.setName(primNodeName);

        // Create MFnMesh
        MFnMesh meshFn;
        MObject meshObj = meshFn.create(
            static_cast<int>(meshData.vertexArray.length()),
            static_cast<int>(meshData.polygonCounts.length()),
            meshData.vertexArray,
            meshData.polygonCounts,
            meshData.polygonConnects,
            primTransformObj,
            &status);

        if (!status || meshObj.isNull())
        {
            return maya_gltf::ReportError("maya_gltf: MFnMesh::create() failed", status);
        }

        meshFn.setName(primNodeName + "Shape#");

        // UV sets (multiple sets from TEXCOORD_0..N)
        if (importOptions_.importUVs)
        {
            for (size_t u = 0; u < meshData.uvSets.size(); ++u)
            {
                auto &uvSet = meshData.uvSets[u];
                if (uvSet.uArray.length() > 0)
                {
                    MString uvSetName = uvSet.name;
                    MStatus uvStatus = meshFn.setUVs(uvSet.uArray, uvSet.vArray, &uvSetName);
                    if (uvStatus)
                    {
                        meshFn.assignUVs(meshData.polygonCounts, uvSet.uvIds, &uvSetName);
                    }
                }
            }
        }

        // Normals
        if (importOptions_.importNormals && meshData.normalArray.length() > 0)
        {
            meshFn.setFaceVertexNormals(
                meshData.normalArray,
                meshData.normalFaceList,
                meshData.normalVertexList);
        }

        // Vertex colors (COLOR_0)
        if (importOptions_.importVertexColors && meshData.hasColors && meshData.colorArray.length() > 0)
        {
            MString colorSetName("colorSet1");
            meshFn.createColorSet(colorSetName);
            MIntArray vertexIds;
            for (unsigned int ci = 0; ci < meshData.colorArray.length(); ++ci)
                vertexIds.append(ci);
            meshFn.setVertexColors(meshData.colorArray, vertexIds);
        }

        // Material
        if (importOptions_.importMaterial)
        {
            if (primitive.material)
            {
                GltfMaterialImporter matImp(document_);
                status = matImp.AssignMaterial(primitive.material, primTransformObj);
            }
            else
            {
                GltfMaterialImporter matImp(document_);
                status = matImp.AssignDefaultMaterial(primTransformObj);
            }
        }

        if (!status)
        {
            maya_gltf::ReportWarning("maya_gltf: material assignment failed for primitive " + MString() + static_cast<int>(p));
        }

        // Record skin info for first primitive
        if (p == 0 && gltfNode && gltfNode->skin && !meshObj.isNull())
        {
            SkinRecord rec;
            rec.skin = gltfNode->skin;
            rec.meshParentObj = primTransformObj; // stable handle to find mesh shape later
            rec.primitive = &primitive;
            rec.vertexMap = meshData.vertexMap;
            pendingSkins_.push_back(std::move(rec));
        }
    } // end for each primitive

    return MS::kSuccess;
}

// ---------------------------------------------------------------------------
// Camera import
// ---------------------------------------------------------------------------

MStatus GltfSceneImporter::importCamera(
    const cgltf_camera *camera,
    MObject parentTransform)
{
    if (!camera) return MS::kSuccess;

    MStatus status;

    // Create a camera shape under the parent transform
    MFnCamera cameraFn;
    MObject cameraObj = cameraFn.create(parentTransform, &status);
    if (!status)
    {
        return maya_gltf::ReportWarning(
            MString("maya_gltf: failed to create camera for ") + camera->name);
    }

    const MString camName = this->sanitizeNodeName(camera->name ? camera->name : "gltf_camera");
    cameraFn.setName(camName);

    if (camera->type == cgltf_camera_type_perspective)
    {
        const auto &persp = camera->data.perspective;
        // Set aspect ratio (if defined)
        if (persp.has_aspect_ratio)
        {
            cameraFn.setAspectRatio(static_cast<double>(persp.aspect_ratio));
        }

        // Set vertical field of view
        const double vfovRad = static_cast<double>(persp.yfov);
        const double aspect = persp.has_aspect_ratio ? static_cast<double>(persp.aspect_ratio) : 1.777777;
        const double hfovDeg = 2.0 * std::atan(std::tan(vfovRad * 0.5) * aspect) * 180.0 / 3.14159265358979323846;
        cameraFn.setHorizontalFieldOfView(hfovDeg);

        // Set clipping planes (znear always present; zfar may be optional per spec)
        cameraFn.setNearClippingPlane(static_cast<double>(persp.znear));
        if (persp.has_zfar)
        {
            cameraFn.setFarClippingPlane(static_cast<double>(persp.zfar));
        }
    }
    else if (camera->type == cgltf_camera_type_orthographic)
    {
        const auto &ortho = camera->data.orthographic;
        // Set orthographic mode via plug attribute
        MPlug orthoPlug = cameraFn.findPlug("orthographic", &status);
        if (!orthoPlug.isNull()) orthoPlug.setBool(true);

        // Set orthographic width
        cameraFn.setOrthoWidth(static_cast<double>(ortho.xmag));

        // Set clipping planes
        cameraFn.setNearClippingPlane(static_cast<double>(ortho.znear));
        cameraFn.setFarClippingPlane(static_cast<double>(ortho.zfar));
    }

    return MS::kSuccess;
}

// ============================================================================
// Skinning (joints + skinCluster)
// ============================================================================

void GltfSceneImporter::buildJointSet(const cgltf_data *data)
{
    jointNodes_.clear();
    if (!data) return;

    for (cgltf_size s = 0; s < data->skins_count; ++s)
    {
        const cgltf_skin &skin = data->skins[s];
        for (cgltf_size j = 0; j < skin.joints_count; ++j)
        {
            if (skin.joints[j])
                jointNodes_.insert(skin.joints[j]);
        }
    }
}

bool GltfSceneImporter::isJoint(const cgltf_node *node) const
{
    return jointNodes_.count(node) > 0;
}

MObject GltfSceneImporter::createNodeAsJoint(const MString &name, MObject parentTransform)
{
    MStatus status;
    MFnIkJoint jointFn;
    MObject jointObj = jointFn.create(parentTransform, &status);
    if (status)
    {
        jointFn.setName(name);

        MPlug sscPlug = jointFn.findPlug("segmentScaleCompensate", true, &status);
        if (status)
        {
            sscPlug.setBool(false);
        }
        status = MS::kSuccess;
    }
    return jointObj;
}


MString GltfSceneImporter::sanitizeNodeName(const std::string &name) const
{
    if (name.empty())
    {
        return MString("gltf_node");
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
        sanitized = "gltf_" + sanitized;
    }

    return MString(sanitized.c_str());
}

