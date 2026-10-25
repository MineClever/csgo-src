#pragma once

#include <common_gltf/cgltf.h>

#include <map>
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDGModifier.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

// ---------------------------------------------------------------------------
// Per-primitive skin record — used to defer skin cluster creation until all
// nodes are in the DAG and the joint set is known.
// ---------------------------------------------------------------------------
struct SkinRecord
{
    const cgltf_skin *skin = nullptr;
    MObject meshParentObj;        // primTransformObj — stable handle to find mesh shape
    const cgltf_primitive *primitive = nullptr;
    std::map<int, int> vertexMap; // glTF → Maya vertex index map from dedup
};

// ---------------------------------------------------------------------------
// Creates skin clusters on previously-imported meshes.
// Receives the imported node path map via constructor injection so it has
// no coupling to GltfSceneImporter state.
// ---------------------------------------------------------------------------
class GltfSkinClusterBuilder
{
public:
    explicit GltfSkinClusterBuilder(
        const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths);

    MStatus SetupSkins(const cgltf_data *data, std::vector<SkinRecord> &pendingSkins);

private:
    const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths_;
};

// ---------------------------------------------------------------------------
// Free helpers used during skin cluster creation (extracted from anonymous
// namespace, now in a named scope).
// ---------------------------------------------------------------------------
MStatus ConnectArrayPlug(
    MDGModifier &modifier,
    const MObject &srcNode,
    const char *srcAttr,
    unsigned int srcIndex,
    const MObject &dstNode,
    const char *dstAttr,
    unsigned int dstIndex);

MStatus CreateSkinClusterWithApi(
    const MDagPathArray &influencePaths,
    const MDagPath &meshDagPath,
    const MDagPath &meshParentPath,
    MObject &skinClusterObject);
