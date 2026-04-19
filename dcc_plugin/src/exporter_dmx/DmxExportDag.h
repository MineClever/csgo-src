#pragma once

// Internal use only - included exclusively by sub-module .cpp files in the exporter.
// Do NOT include this header from other .h files.

#include "DmxExportInternals.h"

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPxFileTranslator.h>

namespace dmx_export_impl
{

simple_dmx::Element *BuildTransformElement(simple_dmx::DocumentBuilder &builder, const MDagPath &dagPath);
simple_dmx::Element *BuildDagElement(simple_dmx::DocumentBuilder &builder, const MDagPath &dagPath, ExportContext &context);
void RegisterDagElementsRecursive(simple_dmx::DocumentBuilder &builder, const MDagPath &dagPath, ExportContext &context);
std::vector<MDagPath> CollectExportRoots(MPxFileTranslator::FileAccessMode mode);

} // namespace dmx_export_impl
