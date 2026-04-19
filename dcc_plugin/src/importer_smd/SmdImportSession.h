#pragma once

#include <common/ImportPolicy.h>
#include <common/ImportTransformCorrection.h>

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

struct SmdImportOptions
{
    dcc_import_transform::TransformCorrection transformCorrection;
    dcc_import_policy::SceneImportPolicy scenePolicy;
    double animationFps = 0.0;
    bool flipUvV = true;
};

class SmdImportSession
{
public:
    SmdImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus validateInputFile() const;
    SmdImportOptions parseOptions() const;

    MFileObject fileObject_;
    MString options_;
};
