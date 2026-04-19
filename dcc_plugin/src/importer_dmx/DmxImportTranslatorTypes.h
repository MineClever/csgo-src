#pragma once

#include <common/ImportPolicy.h>
#include <common/TransformCorrection.h>
#include <common_dmx/SimpleDmxDocument.h>

#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
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
    std::unordered_set<std::string> reusedDagElementKeys;
    std::unordered_set<std::string> reusedTransformElementKeys;
    std::unordered_map<std::string, std::vector<BlendShapeTargetBinding>> importedBlendShapeTargets;
    std::unordered_map<std::string, std::vector<ScalarAttributeBinding>> importedScalarTargets;
    std::vector<MDagPath> importedControlPaths;
    MObject sceneRoot = MObject::kNullObj;
    MMatrix topLevelPreTransform;
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
    dcc_import_transform::TransformCorrection transformCorrection;
};

struct ImportOptions
{
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
    bool applyLegacyAxisCorrection = false;
    dcc_import_transform::TransformCorrection transformCorrection;
    dcc_import_policy::SceneImportPolicy scenePolicy;
};
}
