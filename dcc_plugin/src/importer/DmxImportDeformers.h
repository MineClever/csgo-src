#pragma once

#include "DmxImportTranslatorTypes.h"

#include "../common/SimpleDmxDocument.h"

#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using dmx_import_translator::ImportContext;

MStatus ApplySkinning(
    const ImportContext &context,
    const simple_dmx::Element *vertexData,
    const MObject &meshObject,
    const MObject &meshParentObject);

MStatus ApplyDeltaStates(
    ImportContext &context,
    const simple_dmx::Document &document,
    const simple_dmx::Element *meshElement,
    const MObject &meshObject,
    const MObject &meshParentObject,
    const MPointArray &basePoints);

} // namespace dmx_import_impl
