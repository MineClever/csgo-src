#pragma once

#include "DmxExportTextModel.h"
#include "DmxExportTranslatorTypes.h"

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPointArray.h>

namespace dmx_export_impl
{
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export_translator::ExportContext;

void AppendSkinningData(const MDagPath &meshPath, DmxElement &vertexDataElement, ExportContext &context);
void AppendBlendShapeDeltaStates(
    DmxTextBuilder &builder,
    const MDagPath &meshPath,
    const MPointArray &meshPoints,
    ExportContext &context,
    std::vector<DmxElement *> &deltaStateElements);

} // namespace dmx_export_impl
