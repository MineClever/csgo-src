#pragma once

#include <common/ImportPolicy.h>
#include "../common/SimpleDmxDocument.h"

#include <maya/MDagPath.h>
#include <maya/MObject.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace dmx_import_translator
{
struct BlendShapeTargetBinding
{
    MObject node;
    unsigned int weightIndex = 0;
};

struct ScalarAttributeBinding
{
    MObject node;
    std::string attributeName;
};

struct ImportContext
{
    const simple_dmx::Document &document;
    const simple_dmx::Element *modelRoot = nullptr;
    dcc_import_policy::SceneImportPolicy scenePolicy;
    std::vector<std::string> jointOrder;
    std::unordered_map<std::string, MDagPath> importedDagPaths;
    std::unordered_map<std::string, MDagPath> importedTransformPaths;
    std::unordered_map<std::string, std::vector<BlendShapeTargetBinding>> importedBlendShapeTargets;
    std::unordered_map<std::string, std::vector<ScalarAttributeBinding>> importedScalarTargets;
    std::vector<MDagPath> importedControlPaths;
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
    // When true, a root axis correction rotation is applied to the scene root node if the DMX
    // upAxis differs from the current Maya scene axis. Disable for round-trip Maya→DMX→Maya
    // workflows where the data is already in the correct coordinate space, or when the skinned
    // mesh over-correction (DAG inheritance double-applying the rotation) is undesirable.
    bool applyAxisCorrection = true;
};

struct ImportOptions
{
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
    bool applyAxisCorrection = true;
    dcc_import_policy::SceneImportPolicy scenePolicy;
};
}
