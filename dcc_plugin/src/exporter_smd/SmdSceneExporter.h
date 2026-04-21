#pragma once

#include <common_smd/SimpleSmdDocument.h>
#include <common/TransformCorrection.h>
#include <maya/MDagPath.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>

#include <unordered_map>
#include <vector>

class SmdSceneExporter
{
public:
    SmdSceneExporter(
        MPxFileTranslator::FileAccessMode mode,
        const dcc_export_transform::ExportTransformPolicy &transformPolicy,
        bool exportMesh,
        bool exportAnimation,
        bool flipUvV);

    MStatus Build();
    const simple_smd::Document &document() const;

private:
    MStatus collectExportRoots();
    MStatus collectNodesRecursive(const MDagPath &dagPath, int parentIndex);
    MStatus buildNodes();
    MStatus buildSkeleton();
    MStatus buildTriangles();
    MStatus applyDocumentTransformCorrection();
    void collectAnimationFrameTimes(std::vector<double> &frameTimes) const;
    MStatus collectSkinWeights(
        const MDagPath &meshPath,
        std::unordered_map<int, std::vector<simple_smd::TriangleWeight>> &weightsByVertex) const;

    bool shouldExportRoot(const MDagPath &dagPath) const;
    bool shouldExportNode(const MDagPath &dagPath) const;
    bool isImportWrapperRoot(const MDagPath &dagPath) const;
    bool hasRenderableMeshDescendant(const MDagPath &dagPath) const;
    int findOwningNodeIndex(const MDagPath &dagPath) const;

    MPxFileTranslator::FileAccessMode mode_;
    dcc_export_transform::ExportTransformPolicy transformPolicy_;
    bool exportMesh_ = true;
    bool exportAnimation_ = true;
    bool flipUvV_ = true;
    simple_smd::Document document_;
    std::vector<MDagPath> exportRoots_;
    std::vector<MDagPath> meshRoots_;
    std::vector<MDagPath> exportNodes_;
    std::unordered_map<std::string, int> nodeIndexByPath_;
};
