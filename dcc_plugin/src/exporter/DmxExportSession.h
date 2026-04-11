#pragma once

#include "DmxExportTranslatorTypes.h"

#include <maya/MFileObject.h>
#include <maya/MDagPath.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <vector>

class DmxExportSession
{
public:
    DmxExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode);

    MStatus Run();

private:
    MStatus Initialize();
    MStatus BuildDocument();
    MStatus Serialize();
    MStatus WriteOutput() const;

    MFileObject fileObject_;
    MString optionsText_;
    MPxFileTranslator::FileAccessMode mode_;
    dmx_export_translator::ExportOptions exportOptions_;
    std::vector<MDagPath> exportRoots_;
    std::string serialized_;
};
