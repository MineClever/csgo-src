#pragma once

#include "../common/SimpleDmxDocument.h"
#include "DmxExportTranslatorTypes.h"

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPointArray.h>

namespace dmx_export_impl
{
using dmx_export_translator::ExportContext;

void AppendSkinningData(const MDagPath &meshPath, simple_dmx::Element &vertexDataElement, ExportContext &context);
void AppendBlendShapeDeltaStates(
    simple_dmx::DocumentBuilder &builder,
    const MDagPath &meshPath,
    const MPointArray &meshPoints,
    ExportContext &context,
    std::vector<simple_dmx::Element *> &deltaStateElements);

} // namespace dmx_export_impl
