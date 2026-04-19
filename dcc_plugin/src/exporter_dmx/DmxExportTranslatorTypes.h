#pragma once

#include <common_dmx/SimpleDmxDocument.h>
#include <common/TransformCorrection.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace dmx_export_translator
{
struct ExportContext
{
    std::vector<simple_dmx::Element *> jointElements;
    std::unordered_map<std::string, int> jointIndexByPath;
    std::unordered_map<std::string, simple_dmx::Element *> dagElementByPath;
    std::unordered_map<std::string, simple_dmx::Element *> transformElementByPath;
    std::unordered_map<std::string, simple_dmx::Element *> floatTargetElementByName;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    bool exportMetadata = true;
    std::string materialRoot;
    dcc_export_transform::ExportTransformPolicy transformPolicy;
};

struct ExportOptions
{
    bool binary = false;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    bool exportMetadata = true;
    std::string upAxis = "Y";
    std::string materialRoot;
    dcc_import_transform::TransformCorrection transformCorrection;
};

struct IndexedChannel
{
    std::string formatName;
    std::string valueAttributeName;
    std::string indexAttributeName;
    std::vector<std::string> values;
    std::vector<std::string> indices;
    std::unordered_map<std::string, int> valueMap;
};

struct MeshMaterialData
{
    std::string materialName;
    std::string shadingGroupName;
    std::string shaderName;
    std::string shaderType;
    std::string color;
    std::string transparency;
    std::string diffuseTexture;
    std::string normalTexture;
    std::string bumpTexture;
};
}
