#pragma once

#include "SmdImportSession.h"

#include <common_smd/SimpleSmdDocument.h>

#include <memory>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

struct VtaMeshBinding
{
    MDagPath meshPath;
    MDagPath transformPath;
    MObject blendShapeObject = MObject::kNullObj;
    MPlug weightArrayPlug;
    unsigned int nextWeightIndex = 0;
    std::vector<int> rawToLocalVertexIndex;
};

class VtaSceneImporter
{
public:
    VtaSceneImporter(
        std::shared_ptr<const simple_smd::Document> document,
        const SmdImportOptions &importOptions);

    MStatus Import() const;

private:
    MStatus resolveTargetMeshes(std::vector<VtaMeshBinding> &bindings) const;
    MStatus ensureBlendShapeNode(const MObject &meshObject, MObject &blendShapeObject) const;
    MStatus initializeBlendShapeBinding(VtaMeshBinding &binding) const;
    MStatus createFrameTarget(
        const VtaMeshBinding &binding,
        const simple_smd::VertexAnimationFrame &frame,
        const std::vector<const simple_smd::VertexAnimationSample *> &frameSamples,
        MObject &targetMeshObject,
        MString &targetTransformName) const;

    std::shared_ptr<const simple_smd::Document> document_;
    SmdImportOptions importOptions_;
};
