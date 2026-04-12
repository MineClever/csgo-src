#pragma once

#include "DmxImportTranslatorTypes.h"

#include <common/SimpleDmxDocument.h>

#include <memory>
#include <string>
#include <unordered_map>

#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MString.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using dmx_import_translator::ImportContext;

class DeformerImporter
{
public:
    explicit DeformerImporter(std::shared_ptr<ImportContext> context);

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
    struct ExistingBlendShapeInfo
    {
        MObject node = MObject::kNullObj;
        MString nodeName;
        std::unordered_map<std::string, unsigned int> targetIndicesByAlias;
        unsigned int nextTargetIndex = 0;
    };

    MStatus bindMeshContext(const MObject &meshObject, const MObject &meshParentObject);
    MObject findPrimaryMeshChildForDeformers(const MObject &transformObject) const;
    MObject findExistingSkinClusterNode() const;
    MStatus updateExistingSkinClusterBindings(
        const MObject &skinClusterObject,
        const MDagPathArray &influencePaths) const;
    MStatus createSkinClusterWithApi(const MDagPathArray &influencePaths, MObject &skinClusterObject) const;
    MStatus restoreSkinClusterSettings(const MObject &skinClusterObject) const;
    MObject findExistingBlendShapeNode(const std::string &blendShapeName) const;
    ExistingBlendShapeInfo inspectExistingBlendShape(const MObject &blendShapeObject) const;
    MStatus updateExistingBlendShapeTargetGeometry(
        const MString &blendShapeNodeName,
        unsigned int weightIndex,
        const MPointArray &targetPoints) const;
    void registerBlendShapeTargetBinding(
        const std::string &targetName,
        const dmx_import_translator::BlendShapeTargetBinding &binding);
    MStatus applyBlendShapeAliases(
        const MFnDependencyNode &blendShapeDependency,
        const MString &blendShapeNodeName,
        const std::vector<std::pair<unsigned int, std::string>> &newAliasBindings) const;

    std::shared_ptr<ImportContext> context_;
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
