#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the importer.
// Do NOT include this header from other .h files.

#include "DmxImportTranslatorTypes.h"
#include "DmxImportUtils.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxDocument.h"

#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using simple_dmx::FindAttributeElement;
using simple_dmx::FindAttributeElementArray;
using simple_dmx::FindAttributeString;
using simple_dmx::FindAttributeStringArray;
using simple_dmx::ParseNumberList;
using dmx_import_utils::SanitizeNodeName;

using dmx_import_translator::BlendShapeTargetBinding;
using dmx_import_translator::ImportContext;
using dmx_import_translator::ImportOptions;
using dmx_import_translator::ScalarAttributeBinding;

struct DeltaStateGroup
{
    std::string nodeName;
    std::vector<const simple_dmx::Element *> states;
};

// --- Debug helper (implemented in DmxImportTranslator.cpp) ---
void AppendImportDebugLog(const char *message);

// --- Element helpers ---
std::string ElementKey(const simple_dmx::Element *element);
bool ParseMatrixString(const std::string &text, MMatrix &matrix);
bool ParseFloat3(const std::string &text, float (&components)[3]);

// --- Material / node helpers ---
MStatus SetVector3Plug(const MPlug &plug, const std::string &value);
MStatus ConnectPlugs(MPlug sourcePlug, MPlug destinationPlug);
MObject EnsureDependencyNode(const std::string &nodeType, const std::string &requestedName, MStatus &status);
MObject EnsureShadingGroup(const std::string &requestedName, MStatus &status);
MStatus AssignTextureToShader(
    const std::string &fileNodeName,
    const std::string &texturePath,
    MPlug destinationPlug,
    bool useAlphaOutput);

} // namespace dmx_import_impl
