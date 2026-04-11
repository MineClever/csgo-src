#include "SmdExportSession.h"

#include "../common_smd/MayaSmdCommon.h"

#include <fstream>

SmdExportSession::SmdExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , options_(options)
    , mode_(mode)
    , sceneExporter_(mode)
{
}

MStatus SmdExportSession::Run()
{
    const MStatus validationStatus = validateOutputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    MStatus status = buildDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = serialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = writeOutput();
    if (!status)
    {
        return MStatus::kFailure;
    }

    return maya_smd::ReportInfo(MString("maya_smd: exported text SMD to ") + fileObject_.rawFullName());
}

MStatus SmdExportSession::validateOutputFile() const
{
    if (!maya_smd::HasSmdExtension(fileObject_))
    {
        return maya_smd::ReportError(MString("maya_smd: unsupported export extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

MStatus SmdExportSession::buildDocument()
{
    return sceneExporter_.Build();
}

MStatus SmdExportSession::serialize()
{
    serialized_ = sceneExporter_.document().Serialize();
    if (serialized_.empty())
    {
        return maya_smd::ReportError(MString("maya_smd: exporter produced empty output for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

MStatus SmdExportSession::writeOutput() const
{
    std::ofstream output(fileObject_.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return maya_smd::ReportError(MString("maya_smd: failed to open output file ") + fileObject_.rawFullName());
    }

    output.write(serialized_.data(), static_cast<std::streamsize>(serialized_.size()));
    output.close();
    if (!output)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to write output file ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}
