#pragma once

#include "../common/SimpleDmxDocument.h"
#include "DmxExportTranslatorTypes.h"

#include <string>
#include <vector>

#include <maya/MDagPath.h>

namespace dmx_export_impl
{
using dmx_export_translator::ExportContext;

simple_dmx::Element *BuildAnimationListElement(
    simple_dmx::DocumentBuilder &builder,
    const std::vector<MDagPath> &exportRoots,
    ExportContext &context);

} // namespace dmx_export_impl
