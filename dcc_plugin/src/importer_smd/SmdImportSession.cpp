#include "SmdImportSession.h"
#include "SmdSceneImporter.h"

#include "../common_smd/MayaSmdCommon.h"
#include "../common_smd/SimpleSmdDocument.h"

SmdImportSession::SmdImportSession(const MFileObject &fileObject, const MString &options)
    : fileObject_(fileObject), options_(options)
{
}

MStatus SmdImportSession::Run()
{
    const MStatus validationStatus = validateInputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    simple_smd::Document document;
    std::string errorMessage;
    if (!document.ParseFromFile(fileObject_.resolvedFullName().asChar(), &errorMessage))
    {
        return maya_smd::ReportError(MString("maya_smd: failed to parse SMD file: ") + errorMessage.c_str());
    }

    if (document.nodes.empty())
    {
        return maya_smd::ReportError(MString("maya_smd: SMD file did not contain any nodes: ") + fileObject_.rawFullName());
    }

    SmdSceneImporter importer(document);
    return importer.Import();
}

MStatus SmdImportSession::validateInputFile() const
{
    if (!maya_smd::HasSmdExtension(fileObject_))
    {
        return maya_smd::ReportError(MString("maya_smd: unsupported import extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

MStatus SmdImportSession::reportNotImplemented() const
{
    MString message = maya_smd::MakeStubMessage("import", fileObject_);
    if (options_.length() > 0)
    {
        message += " with options: ";
        message += options_;
    }

    return maya_smd::ReportError(message);
}
