#pragma once

#include <common_gltf/cgltf.h>

#include <memory>
#include <unordered_map>

#include <maya/MDagPath.h>
#include <maya/MStatus.h>

namespace simple_gltf { class Document; }
struct GltfImportOptions;

// ---------------------------------------------------------------------------
// Imports glTF animation channels as Maya animCurves.
// Receives its dependencies (document, imported node paths, import options)
// via constructor injection so it has no coupling to GltfSceneImporter state.
// ---------------------------------------------------------------------------
class GltfAnimationImporter
{
public:
    GltfAnimationImporter(
        std::shared_ptr<const simple_gltf::Document> document,
        const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths,
        const GltfImportOptions &importOptions);

    MStatus ImportAnimations(const cgltf_data *data);

private:
    std::shared_ptr<const simple_gltf::Document> document_;
    const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths_;
    const GltfImportOptions &importOptions_;
};
