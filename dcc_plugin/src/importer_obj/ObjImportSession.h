#pragma once

#include <common/TransformCorrection.h>

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

struct ObjImportOptions
{
    dcc_import_transform::TransformCorrection transformCorrection;
    bool useSceneRoot = false;
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
