#include "GltfSkinningHelper.h"
#include "GltfAccessorReader.h"

#include <common/MayaCommandUtils.h>
#include <common/SkinClusterUtils.h>
#include <common_gltf/MayaGltfCommon.h>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDGModifier.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItGeometry.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

// ============================================================================
// Free helpers
// ============================================================================

MStatus ConnectArrayPlug(
    MDGModifier &modifier,
    const MObject &srcNode,
    const char *srcAttr,
    unsigned int srcIndex,
    const MObject &dstNode,
    const char *dstAttr,
    unsigned int dstIndex)
{
    MStatus status;
    MFnDependencyNode srcFn(srcNode, &status);
    if (!status)
    {
        return status;
    }

    MFnDependencyNode dstFn(dstNode, &status);
    if (!status)
    {
        return status;
    }

    MPlug srcPlug = srcFn.findPlug(srcAttr, true, &status);
    if (!status)
    {
        return status;
    }
    if (srcPlug.isArray())
    {
        srcPlug = srcPlug.elementByLogicalIndex(srcIndex, &status);
        if (!status)
        {
            return status;
        }
    }

    MPlug dstPlug = dstFn.findPlug(dstAttr, true, &status);
    if (!status)
    {
        return status;
    }
    if (dstPlug.isArray())
    {
        dstPlug = dstPlug.elementByLogicalIndex(dstIndex, &status);
        if (!status)
        {
            return status;
        }
    }

    return modifier.connect(srcPlug, dstPlug);
}

MStatus CreateSkinClusterWithApi(
    const MDagPathArray &influencePaths,
    const MDagPath &meshDagPath,
    const MDagPath &meshParentPath,
    MObject &skinClusterObject)
{
    skinClusterObject = MObject::kNullObj;

    MStatus status;
    MFnMesh meshFn(meshDagPath, &status);
    if (!status)
    {
        return status;
    }

    const MString originalShapeName = meshFn.name() + "Orig";
    MObject originalMeshObject = meshFn.copy(meshDagPath.node(), meshParentPath.node(), &status);
    if (!status)
    {
        return status;
    }

    MFnDependencyNode originalMeshNode(originalMeshObject, &status);
    if (!status)
    {
        return status;
    }
    originalMeshNode.setName(originalShapeName, false, &status);

    MPlug intermediatePlug = originalMeshNode.findPlug("intermediateObject", true, &status);
    if (status)
    {
        intermediatePlug.setBool(true);
    }
    status = MS::kSuccess;

    MFnDependencyNode skinClusterNodeFn;
    skinClusterObject = skinClusterNodeFn.create("skinCluster", "gltfSkinCluster#", &status);
    if (!status)
    {
        return status;
    }

    MDGModifier modifier;

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return status;
    }

    MPlug inputPlug = skinClusterNode.findPlug("input", true, &status);
    if (!status)
    {
        return status;
    }
    inputPlug = inputPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return status;
    }
    MPlug inputGeometryPlug = inputPlug.child(0, &status);
    if (!status)
    {
        return status;
    }

    MPlug sourceWorldMeshPlug = originalMeshNode.findPlug("worldMesh", true, &status);
    if (!status)
    {
        return status;
    }
    sourceWorldMeshPlug = sourceWorldMeshPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return status;
    }
    status = modifier.connect(sourceWorldMeshPlug, inputGeometryPlug);
    if (!status)
    {
        return status;
    }

    status = ConnectArrayPlug(modifier, originalMeshObject, "outMesh", 0, skinClusterObject, "originalGeometry", 0);
    if (!status)
    {
        return status;
    }

    status = ConnectArrayPlug(modifier, skinClusterObject, "outputGeometry", 0, meshDagPath.node(), "inMesh", 0);
    if (!status)
    {
        return status;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        status = ConnectArrayPlug(
            modifier,
            influencePaths[influenceIndex].node(),
            "worldMatrix",
            0,
            skinClusterObject,
            "matrix",
            influenceIndex);
        if (!status)
        {
            return status;
        }

        status = dcc_skinning::SetSkinClusterBindPreMatrix(
            skinClusterObject,
            influenceIndex,
            influencePaths[influenceIndex].inclusiveMatrixInverse());
        if (!status)
        {
            return status;
        }
    }

    status = dcc_skinning::SetSkinClusterGeomMatrix(skinClusterObject, meshParentPath.inclusiveMatrix());
    if (!status)
    {
        return status;
    }

    return modifier.doIt();
}

// ============================================================================
// GltfSkinClusterBuilder
// ============================================================================

GltfSkinClusterBuilder::GltfSkinClusterBuilder(
    const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths)
    : importedNodePaths_(importedNodePaths)
{
}

MStatus GltfSkinClusterBuilder::SetupSkins(const cgltf_data *data, std::vector<SkinRecord> &pendingSkins)
{
    if (!data || pendingSkins.empty()) return MS::kSuccess;

    for (auto &rec : pendingSkins)
    {
        const cgltf_skin *skin = rec.skin;
        if (!skin) continue;

        MStatus status;

        if (rec.meshParentObj.isNull())
        {
            maya_gltf::ReportWarning("maya_gltf: skin skipped — null mesh parent");
            continue;
        }

        // Find mesh shape child under parent
        MObject meshObj;
        {
            MFnDagNode parentDag(rec.meshParentObj, &status);
            if (!status)
            {
                maya_gltf::ReportWarning("maya_gltf: skin skipped — invalid mesh parent");
                continue;
            }
            for (unsigned int ci = 0; ci < parentDag.childCount(); ++ci)
            {
                MObject child = parentDag.child(ci);
                if (!child.isNull() && child.hasFn(MFn::kMesh))
                {
                    MFnMesh testMesh(child, &status);
                    if (status && !testMesh.isIntermediateObject())
                    {
                        meshObj = child;
                        break;
                    }
                }
            }
        }
        if (meshObj.isNull())
        {
            maya_gltf::ReportWarning("maya_gltf: skin skipped — mesh shape not found under parent");
            continue;
        }

        MDagPath meshDagPath;
        if (MDagPath::getAPathTo(meshObj, meshDagPath) != MS::kSuccess)
        {
            maya_gltf::ReportWarning("maya_gltf: skin skipped — DAG path resolution failed");
            continue;
        }

        MDagPath meshParentPath = meshDagPath;
        meshParentPath.pop();

        // Step 1: Resolve joint DAG paths
        MDagPathArray jointPaths;
        for (cgltf_size j = 0; j < skin->joints_count; ++j)
        {
            const cgltf_node *jointNode = skin->joints[j];
            if (!jointNode)
            {
                continue;
            }

            const auto jointIt = importedNodePaths_.find(jointNode);
            if (jointIt == importedNodePaths_.end())
            {
                continue;
            }

            jointPaths.append(jointIt->second);
        }

        if (jointPaths.length() == 0)
        {
            maya_gltf::ReportWarning(MString("maya_gltf: no joints found for skinCluster on ") + meshDagPath.fullPathName());
            continue;
        }

        if (!meshDagPath.hasFn(MFn::kMesh))
        {
            maya_gltf::ReportWarning(MString("maya_gltf: skin skipped — DAG path is not a mesh: ") + meshDagPath.fullPathName());
            continue;
        }
        MFnMesh meshFn(meshDagPath, &status);
        if (!status) continue;

        // Step 2: Create skinCluster
        MObject skinClusterObj;
        status = CreateSkinClusterWithApi(jointPaths, meshDagPath, meshParentPath, skinClusterObj);
        if (!status || skinClusterObj.isNull())
        {
            maya_gltf::ReportWarning(MString("maya_gltf: skinCluster API creation failed for ") + meshDagPath.fullPathName());
            continue;
        }

        MFnSkinCluster skinClusterFn(skinClusterObj, &status);
        if (!status) continue;

        // Record bind pose
        status = maya_cmd::EnsureSkinClusterBindPose(jointPaths, meshParentPath);
        if (!status)
        {
            maya_gltf::ReportWarning(MString("maya_gltf: failed to ensure bind pose for ") + meshDagPath.fullPathName());
            status = MS::kSuccess;
        }

        // Step 3: Read JOINTS_0 / WEIGHTS_0 and apply weights
        const cgltf_primitive *prim = rec.primitive;
        if (!prim) continue;

        const cgltf_accessor *jointsAcc = nullptr;
        const cgltf_accessor *weightsAcc = nullptr;
        for (cgltf_size a = 0; a < prim->attributes_count; ++a)
        {
            const cgltf_attribute &attr = prim->attributes[a];
            if (attr.type == cgltf_attribute_type_joints && attr.index == 0)
                jointsAcc = attr.data;
            if (attr.type == cgltf_attribute_type_weights && attr.index == 0)
                weightsAcc = attr.data;
        }

        if (!jointsAcc || !weightsAcc || jointsAcc->count != weightsAcc->count)
            continue;

        if (!jointsAcc->buffer_view || !jointsAcc->buffer_view->buffer)
            continue;
        if (!weightsAcc->buffer_view || !weightsAcc->buffer_view->buffer)
            continue;
        if (!jointsAcc->buffer_view->buffer->data || !weightsAcc->buffer_view->buffer->data)
            continue;

        const cgltf_size gltfVertCount = jointsAcc->count;
        const cgltf_size numComp = cgltf_num_components(jointsAcc->type);
        if (numComp < 1 || numComp > 8) continue;

        {
            const size_t jointBufSize = jointsAcc->buffer_view->buffer->size;
            const size_t jointDataEnd = jointsAcc->buffer_view->offset + jointsAcc->offset
                + gltfVertCount * numComp * sizeof(uint16_t);
            if (jointDataEnd > jointBufSize) continue;
        }

        std::vector<cgltf_uint> allJoints(gltfVertCount * numComp, 0);
        {
            GltfAccessorReader jointsReader(jointsAcc);
            if (!jointsReader.ReadUInts(allJoints, numComp))
                continue;
        }

        std::vector<float> allWeights(gltfVertCount * numComp, 0.0f);
        cgltf_size unpacked = cgltf_accessor_unpack_floats(weightsAcc, allWeights.data(), allWeights.size());
        if (unpacked != allWeights.size()) continue;

        std::map<int, int> mayaToGltf;
        for (const auto &[gltfIdx, mayaIdx] : rec.vertexMap)
            mayaToGltf[mayaIdx] = gltfIdx;

        if (mayaToGltf.empty()) continue;

        MDagPathArray influencePaths;
        skinClusterFn.influenceObjects(influencePaths, &status);
        if (!status || influencePaths.length() == 0)
        {
            continue;
        }

        std::vector<unsigned int> jointToInfluenceSlot(skin->joints_count, static_cast<unsigned int>(-1));
        for (unsigned int jointIndex = 0; jointIndex < jointPaths.length(); ++jointIndex)
        {
            MDagPath matchedInfluencePath;
            if (!dcc_skinning::FindMatchingInfluencePath(
                    dcc_import_policy::SceneImportPolicy(),
                    influencePaths,
                    jointPaths[jointIndex],
                    matchedInfluencePath))
            {
                continue;
            }

            for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
            {
                if (matchedInfluencePath.fullPathName() == influencePaths[influenceIndex].fullPathName())
                {
                    jointToInfluenceSlot[jointIndex] = influenceIndex;
                    break;
                }
            }
        }

        MIntArray influenceIndices;
        status = dcc_skinning::BuildPhysicalInfluenceIndices(influencePaths, influenceIndices);
        if (!status || influenceIndices.length() == 0)
        {
            continue;
        }

        MObject vertexComponent;
        status = dcc_skinning::CreateMeshVertexComponent(meshFn.numVertices(), vertexComponent);
        if (!status)
        {
            continue;
        }

        MFloatArray weights;
        const unsigned int vertexCount = meshFn.numVertices();
        weights.setLength(vertexCount * influenceIndices.length());
        for (unsigned int weightIndex = 0; weightIndex < weights.length(); ++weightIndex)
        {
            weights[weightIndex] = 0.0f;
        }

        MItGeometry geoIt(meshDagPath, &status);
        if (!status) continue;

        int mayaVi = 0;
        for (; !geoIt.isDone(); geoIt.next(), ++mayaVi)
        {
            auto mapIt = mayaToGltf.find(mayaVi);
            if (mapIt == mayaToGltf.end()) continue;
            const cgltf_size gltfVi = static_cast<cgltf_size>(mapIt->second);
            if (gltfVi >= gltfVertCount) continue;

            for (cgltf_size c = 0; c < numComp; ++c)
            {
                const cgltf_size elemIdx = gltfVi * numComp + c;
                const cgltf_size jointIdx = static_cast<cgltf_size>(allJoints[elemIdx]);

                if (jointIdx >= jointToInfluenceSlot.size()) continue;
                const unsigned int influenceSlot = jointToInfluenceSlot[jointIdx];
                if (influenceSlot == static_cast<unsigned int>(-1) || influenceSlot >= influenceIndices.length()) continue;

                const float w = allWeights[elemIdx];
                if (w <= 0.0001f) continue;

                weights[mayaVi * influenceIndices.length() + influenceSlot] += w;
            }
        }

        const unsigned int maxAssignedInfluences = dcc_skinning::NormalizeWeightBufferInPlace(
            weights,
            vertexCount,
            static_cast<unsigned int>(influenceIndices.length()));

        status = dcc_skinning::PrepareSkinClusterForSetWeights(skinClusterObj, maxAssignedInfluences);
        if (!status)
        {
            continue;
        }

        status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
        if (!status)
        {
            maya_gltf::ReportWarning(MString("maya_gltf: failed to apply skin weights for ") + meshDagPath.fullPathName());
        }
    }

    pendingSkins.clear();
    return MS::kSuccess;
}
