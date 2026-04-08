#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the exporter.
// Do NOT include this header from other .h files.

#include "DmxExportInternals.h"

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPxFileTranslator.h>

namespace dmx_export_impl
{

DmxElement *BuildTransformElement(DmxTextBuilder &builder, const MDagPath &dagPath);
DmxElement *BuildDagElement(DmxTextBuilder &builder, const MDagPath &dagPath, ExportContext &context);
void RegisterDagElementsRecursive(DmxTextBuilder &builder, const MDagPath &dagPath, ExportContext &context);
std::vector<MDagPath> CollectExportRoots(MPxFileTranslator::FileAccessMode mode);

} // namespace dmx_export_impl
