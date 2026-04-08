#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the exporter.
// Do NOT include this header from other .h files.

#include "../common/SimpleDmxDocument.h"
#include "DmxExportTranslatorTypes.h"

#include <array>
#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

namespace dmx_export_impl
{
using simple_dmx::Attribute;
using simple_dmx::ClearAttrs;
using simple_dmx::DocumentBuilder;
using simple_dmx::Element;
using simple_dmx::ScalarAttr;
using simple_dmx::ScalarArrayAttr;
using simple_dmx::SetAttr;
using dmx_export_translator::ExportContext;
using dmx_export_translator::ExportOptions;
using dmx_export_translator::IndexedChannel;
using dmx_export_translator::MeshMaterialData;

// Find a named attribute; returns nullptr if not present.
inline const Attribute *FindAttribute(const Element &element, const char *attributeName)
{
    auto it = element.attributes.find(attributeName);
    return it != element.attributes.end() ? &it->second : nullptr;
}

// Return element.name directly (stored as a field in simple_dmx::Element).
inline std::string GetElementName(const Element &element)
{
    return element.name;
}

// Shallow-clone an element: same type, same name, copy all attributes.
inline Element *CloneElement(DocumentBuilder &builder, const Element &source)
{
    Element *clone = builder.CreateElement(source.type, source.name);
    clone->attributes = source.attributes;
    clone->attributeOrder = source.attributeOrder;
    return clone;
}

// --- Formatting helpers (implemented in DmxExportInternals.cpp) ---
std::string FormatFloat(double value);
std::string FormatVector2(double x, double y);
std::string FormatVector3(double x, double y, double z);
std::string FormatVector4(double x, double y, double z, double w);
std::string FormatQuaternion(double x, double y, double z, double w);
std::string FormatMatrix(const MMatrix &matrix);
std::string FormatTimeSeconds(double value);
std::vector<double> ParseNumberList(const std::string &text);

// --- Plug / node helpers ---
std::string ReadStringPlugValue(const MPlug &plug);
std::string ReadMatrixPlugValue(const MPlug &plug);
bool ReadVector3PlugValue(const MPlug &plug, std::string &formattedValue);
MObject FindConnectedSourceNode(const MPlug &destinationPlug);
std::string FindTexturePathFromPlug(const MPlug &plug);

// --- DAG / mesh helpers ---
std::string DagPathKey(const MDagPath &dagPath);
bool TryGetMeshPathFromObject(const MObject &nodeObject, MDagPath &meshPath);
bool TryRegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MDagPath &targetPath,
    MString &temporaryTransformName);

// --- Animation curve helpers ---
MObject FindAnimationCurveForPlug(const MPlug &plug);
void AppendCurveTimes(const MObject &curveObject, std::vector<double> &times);
double EvaluateCurveOrValue(const MObject &curveObject, const MPlug &plug, double timeSeconds);

} // namespace dmx_export_impl
