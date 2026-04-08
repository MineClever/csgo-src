#pragma once

#include "DmxExportTextModel.h"
#include "DmxExportTranslatorTypes.h"

#include <string>
#include <vector>

#include <maya/MDagPath.h>

namespace dmx_export_impl
{
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export_translator::ExportContext;

DmxElement *BuildAnimationListElement(
    DmxTextBuilder &builder,
    const std::vector<MDagPath> &exportRoots,
    ExportContext &context);

} // namespace dmx_export_impl
