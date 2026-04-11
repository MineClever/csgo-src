#pragma once

#include "DmxImportTranslatorTypes.h"

#include "../common/SimpleDmxDocument.h"

#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using dmx_import_translator::ImportContext;

class DeformerImporter
{
public:
    explicit DeformerImporter(ImportContext &context);

    MStatus ApplySkinning(
        const simple_dmx::Element *vertexData,
        const MObject &meshObject,
        const MObject &meshParentObject);

    MStatus ApplyDeltaStates(
        const simple_dmx::Document &document,
        const simple_dmx::Element *meshElement,
        const MObject &meshObject,
        const MObject &meshParentObject,
        const MPointArray &basePoints);

private:
    MStatus bindMeshContext(const MObject &meshObject, const MObject &meshParentObject);
    MObject findPrimaryMeshChildForDeformers(const MObject &transformObject) const;
    MStatus createSkinClusterWithApi(const MDagPathArray &influencePaths, MObject &skinClusterObject) const;
    MStatus restoreSkinClusterSettings(const MObject &skinClusterObject) const;

    ImportContext &context_;
    const simple_dmx::Document *document_ = nullptr;
    const simple_dmx::Element *meshElement_ = nullptr;
    const simple_dmx::Element *vertexData_ = nullptr;
    MObject meshObject_ = MObject::kNullObj;
    MObject meshParentObject_ = MObject::kNullObj;
    MDagPath meshDagPath_;
    MDagPath meshParentPath_;
    const MPointArray *basePoints_ = nullptr;
};

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
