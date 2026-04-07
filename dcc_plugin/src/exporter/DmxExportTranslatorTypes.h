#pragma once

#include "DmxExportTextModel.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace dmx_export_translator
{
struct ExportContext
{
    std::vector<dmx_export::DmxElement *> jointElements;
    std::unordered_map<std::string, int> jointIndexByPath;
    std::unordered_map<std::string, dmx_export::DmxElement *> dagElementByPath;
    std::unordered_map<std::string, dmx_export::DmxElement *> transformElementByPath;
    std::unordered_map<std::string, dmx_export::DmxElement *> floatTargetElementByName;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    bool exportMetadata = true;
    std::string materialRoot;
};

struct ExportOptions
{
    bool binary = false;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    bool exportMetadata = true;
    std::string upAxis = "Y";
    std::string materialRoot;
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
