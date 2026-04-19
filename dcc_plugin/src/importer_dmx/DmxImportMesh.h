#pragma once

// Internal use only - included exclusively by sub-module .cpp files in the importer.
// Do NOT include this header from other .h files.

#include "DmxImportInternals.h"

#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{

MStatus CreateMeshShape(ImportContext &context, const simple_dmx::Element *dagElement, MObject parent);

} // namespace dmx_import_impl
