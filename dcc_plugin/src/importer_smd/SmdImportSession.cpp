#include "SmdImportSession.h"
#include "SmdSceneImporter.h"

#include "../common_smd/MayaSmdCommon.h"
#include "../common_smd/SimpleSmdDocument.h"

#include <string>

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

    const SmdImportOptions importOptions = parseOptions();
    SmdSceneImporter importer(document, importOptions);
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

SmdImportOptions SmdImportSession::parseOptions() const
{
    SmdImportOptions parsedOptions;
    if (options_.length() == 0)
    {
        return parsedOptions;
    }

    const std::string optionString = options_.asChar();
    size_t optionStart = 0;
    while (optionStart < optionString.size())
    {
        const size_t optionEnd = optionString.find(';', optionStart);
        const std::string option = optionString.substr(
            optionStart,
            optionEnd == std::string::npos ? std::string::npos : optionEnd - optionStart);
        const size_t separator = option.find('=');
        if (separator != std::string::npos && separator + 1 < option.size())
        {
            const std::string key = option.substr(0, separator);
            const std::string value = option.substr(separator + 1);

            try
            {
                const double numericValue = std::stod(value);
                if (key == "rotateX")
                {
                    parsedOptions.rotateXDegrees = numericValue;
                }
                else if (key == "rotateY")
                {
                    parsedOptions.rotateYDegrees = numericValue;
                }
                else if (key == "rotateZ")
                {
                    parsedOptions.rotateZDegrees = numericValue;
                }
            }
            catch (...)
            {
            }
        }

        if (optionEnd == std::string::npos)
        {
            break;
        }
        optionStart = optionEnd + 1;
    }

    return parsedOptions;
}
