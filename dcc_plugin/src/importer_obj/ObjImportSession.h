#pragma once

#include <common/ImportPolicy.h>
#include <common/TransformCorrection.h>

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

struct ObjImportOptions
{
    dcc_import_transform::TransformCorrection transformCorrection;
    dcc_import_policy::SceneImportPolicy scenePolicy;
    bool flipUvV = true;
};

class ObjImportSession
{
public:
    ObjImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus validateInputFile() const;
    ObjImportOptions parseOptions() const;

    MFileObject fileObject_;
    MString options_;
};
