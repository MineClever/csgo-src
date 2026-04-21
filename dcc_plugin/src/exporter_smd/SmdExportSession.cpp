#include "SmdExportSession.h"

#include <common/TransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace smd_export_session_detail
{
std::unordered_map<std::string, std::string> ParseOptionMap(const MString &options)
{
    std::unordered_map<std::string, std::string> optionMap;
    std::string text = options.asChar();
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find(';', start);
        if (end == std::string::npos)
        {
            end = text.size();
        }

        const std::string pair = text.substr(start, end - start);
        const size_t separator = pair.find('=');
        if (separator != std::string::npos)
        {
            std::string key = pair.substr(0, separator);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            optionMap[key] = pair.substr(separator + 1);
        }

        start = end + 1;
    }

    return optionMap;
}

dcc_export_transform::ExportTransformPolicy BuildSmdExportTransformPolicy(const MString &options)
{
    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    return dcc_export_transform::BuildExportTransformPolicy(dcc_import_transform::ParseTransformCorrection(optionMap));
}

bool ParseBoolOption(const std::unordered_map<std::string, std::string> &optionMap, const char *key, bool defaultValue)
{
    const auto it = optionMap.find(key);
    if (it == optionMap.end())
    {
        return defaultValue;
    }

    return it->second == "1" || it->second == "true" || it->second == "True";
}
} // namespace smd_export_session_detail

using namespace smd_export_session_detail;

SmdExportSession::SmdExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , options_(options)
    , mode_(mode)
    , sceneExporter_(
        mode,
        BuildSmdExportTransformPolicy(options),
        parseBoolOption("exportmesh", true),
        parseBoolOption("exportanimation", true),
        parseBoolOption("flipuvv", true),
        parseBoolOption("useexportnameoverride", false))
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

bool SmdExportSession::parseBoolOption(const char *key, bool defaultValue) const
{
    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options_);
    return ParseBoolOption(optionMap, key, defaultValue);
}

MStatus SmdExportSession::buildDocument()
{
    if (!parseBoolOption("exportmesh", true) && !parseBoolOption("exportanimation", true))
    {
        return maya_smd::ReportError("maya_smd: exportMesh and exportAnimation cannot both be disabled.");
    }

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
