#pragma once

#include <common/TransformCorrection.h>

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

// Forward declaration
namespace simple_gltf { class Document; }

struct GltfImportOptions
{
    // Data to import
    bool importMesh = true;
    bool importAnimation = true;
    bool importSkin = true;
    bool importCamera = true;
    bool importMaterial = true;
    bool importVertexColors = true;
    bool importNormals = true;
    bool importUVs = true;

    // Scene setup
    dcc_import_transform::TransformCorrection transformCorrection;
    bool useSceneRoot = false;
    bool restoreDefaultPoseAfterSkin = true;

    // Color management
    bool enableColorManagement = true;

    // Frame rate options for animation import
    bool detectFrameRate = true;   // true = auto-detect from keyframe spacing
    float frameRate = 30.0f;       // custom frame rate (used when detectFrameRate=false)
};

class GltfImportSession
{
public:
    GltfImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus validateInputFile() const;
    GltfImportOptions parseOptions() const;

    MFileObject fileObject_;
    MString options_;
};
