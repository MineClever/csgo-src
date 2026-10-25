#pragma once

#include "GltfImportSession.h"

#include <common/TransformCorrection.h>

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include "GltfSkinningHelper.h"
// Forward declarations
struct cgltf_data;
struct cgltf_node;
struct cgltf_mesh;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_camera;
struct cgltf_skin;
struct cgltf_accessor;

namespace simple_gltf { class Document; }

struct GltfImportOptions;
class GltfSceneImporter
{
public:
    GltfSceneImporter(
        std::shared_ptr<const simple_gltf::Document> document,
        const GltfImportOptions &importOptions);

    MStatus Import();

private:
    // Scene hierarchy
    MStatus importScene(const cgltf_data *data);
    MStatus createNodeHierarchy(const cgltf_node *node, MObject parentTransform);
    MStatus applyNodeTransform(const cgltf_node *node, MObject &transformObj);
    bool isTopLevelNode(const cgltf_node *node) const;

    // Mesh / primitive
    MStatus importMesh(const cgltf_mesh *mesh, const std::string &nodeName, MObject parentTransform, const cgltf_node *gltfNode = nullptr);

    // Camera
    MStatus importCamera(const cgltf_camera *camera, MObject parentTransform);

    // Skinning
    void buildJointSet(const cgltf_data *data);
    bool isJoint(const cgltf_node *node) const;
    MObject createNodeAsJoint(const MString &name, MObject parentTransform);

    // Utilities
    MString sanitizeNodeName(const std::string &name) const;

    // Members
    std::shared_ptr<const simple_gltf::Document> document_;
    GltfImportOptions importOptions_;
    MObject importRoot_ = MObject::kNullObj;

    // Joint set: which cgltf_node pointers are joints
    std::set<const cgltf_node*> jointNodes_;

    std::vector<SkinRecord> pendingSkins_;

    // Stable lookup used by skinning/animation import. Keyed by cgltf node
    // pointer instead of sanitized name so duplicate names do not corrupt
    // skeleton or animation binding.
    std::unordered_map<const cgltf_node *, MDagPath> importedNodePaths_;
};
