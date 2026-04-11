#pragma once

#include "../common/SimpleDmxDocument.h"
#include "DmxExportTranslatorTypes.h"

#include <memory>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MString.h>

namespace dmx_export_impl
{
using dmx_export_translator::ExportContext;

class DeformerExporter
{
public:
    explicit DeformerExporter(std::shared_ptr<ExportContext> context);

    void AppendSkinningData(const MDagPath &meshPath, simple_dmx::Element &vertexDataElement);
    void AppendBlendShapeDeltaStates(
        simple_dmx::DocumentBuilder &builder,
        const MDagPath &meshPath,
        const MPointArray &meshPoints,
        std::vector<simple_dmx::Element *> &deltaStateElements);

private:
    void bindMeshContext(const MDagPath &meshPath);
    std::shared_ptr<ExportContext> context_;
    simple_dmx::DocumentBuilder *builder_ = nullptr;
    simple_dmx::Element *vertexDataElement_ = nullptr;
    std::vector<simple_dmx::Element *> *deltaStateElements_ = nullptr;
    MDagPath meshPath_;
    const MPointArray *meshPoints_ = nullptr;
    MObject currentSkinClusterObject_ = MObject::kNullObj;
    MObject currentBlendShapeObject_ = MObject::kNullObj;
    MString currentBlendShapeNodeName_;
};

void AppendSkinningData(const MDagPath &meshPath, simple_dmx::Element &vertexDataElement, ExportContext &context);
void AppendBlendShapeDeltaStates(
    simple_dmx::DocumentBuilder &builder,
    const MDagPath &meshPath,
    const MPointArray &meshPoints,
    ExportContext &context,
    std::vector<simple_dmx::Element *> &deltaStateElements);

} // namespace dmx_export_impl
