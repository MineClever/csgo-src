#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

class SmdImportSession
{
public:
    SmdImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus validateInputFile() const;
    MStatus reportNotImplemented() const;

    MFileObject fileObject_;
    MString options_;
};
