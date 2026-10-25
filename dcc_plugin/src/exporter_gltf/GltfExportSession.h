#pragma once

#include <maya/MFileObject.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

class GltfSceneExporter;

struct GltfExportOptions
{
    // Data to export
    bool exportMesh = true;
    bool exportAnimation = true;
    bool exportCamera = true;
    bool exportSkin = true;
    bool exportMaterial = true;
    bool exportVertexColors = true;
    bool exportNormals = true;
    bool exportUVs = true;

    // Transform corrections
    bool flipUvV = true;
    float translateX = 0.0f;
    float translateY = 0.0f;
    float translateZ = 0.0f;
    float rotateX = 0.0f;
    float rotateY = 0.0f;
    float rotateZ = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;
};

class GltfExportSession
{
public:
    GltfExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode);

    MStatus Run();

private:
    MStatus validateOutputFile() const;
    GltfExportOptions parseOptions() const;

    MFileObject fileObject_;
    MString options_;
    MPxFileTranslator::FileAccessMode mode_;
};
