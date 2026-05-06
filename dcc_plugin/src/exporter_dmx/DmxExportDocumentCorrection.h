#pragma once

#include <common/TransformCorrection.h>
#include <common_dmx/SimpleDmxDocument.h>

#include <maya/MStatus.h>

namespace dmx_export_impl
{

MStatus ApplyDocumentTransformCorrection(
    simple_dmx::Element *modelElement,
    const dcc_export_transform::ExportTransformPolicy &policy);

} // namespace dmx_export_impl
