#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the exporter.
// Do NOT include this header from other .h files.

#include "DmxExportInternals.h"

#include <maya/MDagPath.h>

namespace dmx_export_impl
{

simple_dmx::Element *BuildMeshElement(simple_dmx::DocumentBuilder &builder, const MDagPath &meshPath, ExportContext &context, const MDagPath *bindShapeMeshPath = nullptr);

} // namespace dmx_export_impl
