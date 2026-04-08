#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the exporter.
// Do NOT include this header from other .h files.

#include "DmxExportTextModel.h"
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
using dmx_export::CloneElement;
using dmx_export::DmxAttribute;
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export::FindAttribute;
using dmx_export::GetElementName;
using dmx_export::MakeElementArrayAttribute;
using dmx_export::MakeInlineElementAttribute;
using dmx_export::MakeScalarArrayAttribute;
using dmx_export::MakeScalarAttribute;
using dmx_export_translator::ExportContext;
using dmx_export_translator::ExportOptions;
using dmx_export_translator::IndexedChannel;
using dmx_export_translator::MeshMaterialData;

// --- Formatting helpers (implemented in DmxExportTranslator.cpp) ---
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
