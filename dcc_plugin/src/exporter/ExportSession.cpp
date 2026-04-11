#include "ExportSession.h"

#include "DmxExportAnimation.h"
#include "DmxExportDag.h"
#include "DmxExportInternals.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxWrite.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <Windows.h>

namespace dmx_export_impl
{
void AppendDebugLog(const char *message)
{
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }

    std::string logPath(tempPath);
    logPath += "maya_dmx_export_debug.log";

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        return;
    }

    logFile << message << "\n";
}
}

namespace
{
using dmx_export_translator::ExportContext;
using dmx_export_translator::ExportOptions;
using namespace dmx_export_impl;

bool IsBinaryExportRequested(const MFileObject &fileObject, const MString &options)
{
    const std::string lowerPath = fileObject.rawFullName().toLowerCase().asChar();
    if (lowerPath.size() >= 5 && lowerPath.substr(lowerPath.size() - 5) == ".dmxb")
    {
        return true;
    }
    if (lowerPath.size() >= 7 && lowerPath.substr(lowerPath.size() - 7) == ".dmxbin")
    {
        return true;
    }

    std::string lowerOptions = options.asChar();
    std::transform(lowerOptions.begin(), lowerOptions.end(), lowerOptions.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowerOptions.find("binary=1") != std::string::npos ||
        lowerOptions.find("binary=true") != std::string::npos ||
        lowerOptions.find("encoding=binary") != std::string::npos;
}

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
            std::string value = pair.substr(separator + 1);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            optionMap[key] = value;
        }

        start = end + 1;
    }
    return optionMap;
}

bool ParseBoolOption(const std::unordered_map<std::string, std::string> &optionMap, const char *key, bool defaultValue)
{
    auto it = optionMap.find(key);
    if (it == optionMap.end())
    {
        return defaultValue;
    }

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "1" || value == "true" || value == "yes";
}

ExportOptions ParseExportOptions(const MFileObject &fileObject, const MString &options)
{
    ExportOptions exportOptions;
    exportOptions.binary = IsBinaryExportRequested(fileObject, options);

    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    exportOptions.exportSkin = ParseBoolOption(optionMap, "exportskin", true);
    exportOptions.exportDeltaStates = ParseBoolOption(optionMap, "exportdeltastates", true);
    exportOptions.exportMetadata = ParseBoolOption(optionMap, "exportmetadata", true);

    auto upAxisIt = optionMap.find("upaxis");
    if (upAxisIt != optionMap.end() && !upAxisIt->second.empty())
    {
        exportOptions.upAxis = upAxisIt->second;
    }

    auto materialRootIt = optionMap.find("materialroot");
    if (materialRootIt != optionMap.end())
    {
        exportOptions.materialRoot = materialRootIt->second;
    }

    return exportOptions;
}
} // namespace

ExportSession::ExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , optionsText_(options)
    , mode_(mode)
{
}

MStatus ExportSession::Run()
{
    AppendDebugLog("writer: begin");

    MStatus status = Initialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = BuildDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = Serialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = WriteOutput();
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendDebugLog("writer: wrote file");
    return maya_dmx::ReportInfo(
        MString(exportOptions_.binary ? "maya_dmx: exported binary DMX to " : "maya_dmx: exported text DMX to ") +
        fileObject_.rawFullName());
}

MStatus ExportSession::Initialize()
{
    exportOptions_ = ParseExportOptions(fileObject_, optionsText_);
    exportRoots_ = CollectExportRoots(mode_);
    AppendDebugLog("writer: collected roots");
    if (exportRoots_.empty())
    {
        AppendDebugLog("writer: no roots");
        return maya_dmx::ReportError("maya_dmx: nothing to export.");
    }

    return MStatus::kSuccess;
}

MStatus ExportSession::BuildDocument()
{
    simple_dmx::DocumentBuilder builder;
    ExportContext context;
    context.exportSkin = exportOptions_.exportSkin;
    context.exportDeltaStates = exportOptions_.exportDeltaStates;
    context.exportMetadata = exportOptions_.exportMetadata;
    context.materialRoot = exportOptions_.materialRoot;

    simple_dmx::Element *modelElement = builder.CreateElement("DmeModel", "maya_export");
    SetAttr(*modelElement, "upAxis", ScalarAttr("string", exportOptions_.upAxis));
    if (exportOptions_.exportMetadata && !exportOptions_.materialRoot.empty())
    {
        SetAttr(*modelElement, "mayaMaterialRoot", ScalarAttr("string", exportOptions_.materialRoot));
    }

    std::vector<simple_dmx::Element *> rootChildren;
    for (const MDagPath &rootPath : exportRoots_)
    {
        RegisterDagElementsRecursive(builder, rootPath, context);
    }
    for (const MDagPath &rootPath : exportRoots_)
    {
        if (simple_dmx::Element *child = BuildDagElement(builder, rootPath, context))
        {
            rootChildren.push_back(child);
        }
    }
    AppendDebugLog("writer: built dag elements");

    if (!rootChildren.empty())
    {
        SetAttr(*modelElement, "children", builder.ElementRefArray(rootChildren));
    }
    if (!context.jointElements.empty())
    {
        SetAttr(*modelElement, "jointList", builder.ElementRefArray(context.jointElements));
    }
    if (simple_dmx::Element *animationListElement = BuildAnimationListElement(builder, exportRoots_, context))
    {
        SetAttr(*modelElement, "animationList", builder.ElementRef(animationListElement));
    }

    builder.SetRoot(modelElement);
    const simple_dmx::Document document = builder.Build();

    std::string serializeError;
    if (exportOptions_.binary)
    {
        if (!simple_dmx::SerializeDocumentBinary(document, serialized_, serializeError))
        {
            AppendDebugLog("writer: binary serialize failed");
            return maya_dmx::ReportError(serializeError.c_str());
        }
    }
    else
    {
        serialized_ = simple_dmx::SerializeDocumentText(document);
    }

    AppendDebugLog("writer: serialized");
    return MStatus::kSuccess;
}

MStatus ExportSession::Serialize()
{
    return MStatus::kSuccess;
}

MStatus ExportSession::WriteOutput() const
{
    std::ofstream output(fileObject_.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to open output file ") + fileObject_.rawFullName());
    }

    output.write(serialized_.data(), static_cast<std::streamsize>(serialized_.size()));
    output.close();
    if (!output)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to write output file ") + fileObject_.rawFullName());
    }

    return MStatus::kSuccess;
}
