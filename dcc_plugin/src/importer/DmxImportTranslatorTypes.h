#pragma once

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
    std::vector<std::string> jointOrder;
    std::unordered_map<std::string, MDagPath> importedDagPaths;
    std::unordered_map<std::string, MDagPath> importedTransformPaths;
    std::unordered_map<std::string, std::vector<BlendShapeTargetBinding>> importedBlendShapeTargets;
    std::unordered_map<std::string, std::vector<ScalarAttributeBinding>> importedScalarTargets;
    std::vector<MDagPath> importedControlPaths;
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
};

struct ImportOptions
{
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
};
}
