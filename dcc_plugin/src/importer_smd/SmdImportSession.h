#pragma once

#include <common/ImportPolicy.h>

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

struct SmdImportOptions
{
    double rotateXDegrees = 0.0;
    double rotateYDegrees = 0.0;
    double rotateZDegrees = 0.0;
    dcc_import_policy::SceneImportPolicy scenePolicy;
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
