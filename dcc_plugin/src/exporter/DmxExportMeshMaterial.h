#pragma once

#include "DmxExportTextModel.h"
#include "DmxExportTranslatorTypes.h"

#include <vector>

#include <maya/MObject.h>

namespace dmx_export_impl
{
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export_translator::MeshMaterialData;

std::vector<int> BuildPolygonRange(int polygonCount);
std::vector<int> ExtractPolygonIndices(const MObject &componentObject);
std::vector<int> FilterUncoveredPolygons(const std::vector<int> &polygonIndices, std::vector<bool> &coveredPolygons);
std::vector<std::string> BuildFaceValues(
    const std::vector<int> &polygonIndices,
    const std::vector<std::vector<int>> &polygonFaceIndices);
void AppendFaceSetElement(
    DmxTextBuilder &builder,
    const char *faceSetName,
    const std::vector<int> &polygonIndices,
    const std::vector<std::vector<int>> &polygonFaceIndices,
    const MeshMaterialData *materialData,
    bool exportMetadata,
    std::vector<DmxElement *> &faceSetElements);
MeshMaterialData BuildMaterialData(const MObject &setObject, const std::string &fallbackName);

} // namespace dmx_export_impl
