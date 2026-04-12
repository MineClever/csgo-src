#include "SmdImportSession.h"
#include "SmdSceneImporter.h"

#include <common_smd/MayaSmdCommon.h>
#include <common_smd/SimpleSmdDocument.h>

#include <memory>
#include <string>
#include <unordered_map>

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

    auto document = std::make_shared<simple_smd::Document>();
    std::string errorMessage;
    if (!document->ParseFromFile(fileObject_.resolvedFullName().asChar(), &errorMessage))
    {
        return maya_smd::ReportError(MString("maya_smd: failed to parse SMD file: ") + errorMessage.c_str());
    }

    if (document->nodes.empty())
    {
        return maya_smd::ReportError(MString("maya_smd: SMD file did not contain any nodes: ") + fileObject_.rawFullName());
    }

    const SmdImportOptions importOptions = parseOptions();
    if (dcc_import_policy::UsesUpdateCurrentScene(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=update now reuses matching hierarchy and can overwrite reused bind pose/base animation, but mesh overwrite is still not implemented yet.");
    }
    else if (dcc_import_policy::UsesAppendMissingObjects(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=append currently reuses matching hierarchy and existing mesh groups, but full scene-merge behavior is not implemented yet.");
    }
    else if (dcc_import_policy::UsesAnimationOnlyImport(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=animationOnly is parsed but not implemented yet; falling back to create-new import behavior.");
    }

    if (importOptions.scenePolicy.importAnimationToLayer)
    {
        maya_smd::ReportWarning("maya_smd: animation layer import options are parsed but not implemented yet; imported animation will still target the base scene.");
    }

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

    const std::unordered_map<std::string, std::string> optionMap = dcc_import_policy::ParseOptionMap(options_);
    parsedOptions.scenePolicy = dcc_import_policy::ParseSceneImportPolicy(optionMap);
    dcc_import_policy::CaptureCurrentNamespace(parsedOptions.scenePolicy);
    for (const auto &entry : optionMap)
    {
        try
        {
            const double numericValue = std::stod(entry.second);
            if (entry.first == "rotatex")
            {
                parsedOptions.rotateXDegrees = numericValue;
            }
            else if (entry.first == "rotatey")
            {
                parsedOptions.rotateYDegrees = numericValue;
            }
            else if (entry.first == "rotatez")
            {
                parsedOptions.rotateZDegrees = numericValue;
            }
        }
        catch (...)
        {
        }
    }

    return parsedOptions;
}
