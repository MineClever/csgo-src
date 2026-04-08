#pragma once

#include "DmxImportTranslatorTypes.h"

#include "../common/SimpleDmxDocument.h"

#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using dmx_import_translator::ImportContext;

const simple_dmx::Element *FindAnimationList(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot);

const simple_dmx::Element *FindCombinationOperator(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot);

MStatus ApplyChannelsClipAnimation(ImportContext &context, const simple_dmx::Element *channelsClip);
MStatus CreateCombinationControls(
    ImportContext &context,
    const simple_dmx::Element *combinationOperator,
    const MObject &sceneRoot);

} // namespace dmx_import_impl
