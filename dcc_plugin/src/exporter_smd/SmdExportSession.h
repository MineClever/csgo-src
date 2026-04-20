#pragma once

#include "SmdSceneExporter.h"

#include <maya/MFileObject.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

class SmdSceneExporter;

class SmdExportSession
{
public:
    SmdExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode);

    MStatus Run();

private:
    MStatus validateOutputFile() const;
    MStatus buildDocument();
    MStatus serialize();
    MStatus writeOutput() const;
    bool parseBoolOption(const char *key, bool defaultValue) const;

    MFileObject fileObject_;
    MString options_;
    MPxFileTranslator::FileAccessMode mode_;
    SmdSceneExporter sceneExporter_;
    std::string serialized_;
};
