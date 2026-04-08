#include "DmxExportTranslator.h"
#include "DmxExportTextModel.h"
#include "DmxExportTranslatorTypes.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxDocument.h"
#include "../common/SimpleDmxWrite.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MFileObject.h>
#include <maya/MString.h>

#include <Windows.h>

#include "DmxExportAnimation.h"
#include "DmxExportDag.h"
#include "DmxExportInternals.h"

namespace dmx_export_impl
{
using dmx_export::CloneElement;
using dmx_export::DmxAttribute;
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export::FindAttribute;
using dmx_export::GetElementName;
using dmx_export::MakeElementArrayAttribute;
using dmx_export::MakeInlineElementAttribute;
using dmx_export::MakeScalarArrayAttribute;
using dmx_export::MakeScalarAttribute;
using dmx_export_translator::ExportContext;
using dmx_export_translator::ExportOptions;
using dmx_export_translator::IndexedChannel;
using dmx_export_translator::MeshMaterialData;

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
} // namespace dmx_export_impl

using namespace dmx_export_impl;

void *DmxExportTranslator::Create()
{
    return new DmxExportTranslator();
}

bool DmxExportTranslator::haveReadMethod() const
{
    return false;
}

bool DmxExportTranslator::haveWriteMethod() const
{
    return true;
}

MString DmxExportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxExportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxExportTranslator::writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode)
{
    try
    {
        AppendDebugLog("writer: begin");
        const ExportOptions exportOptions = ParseExportOptions(fileObject, options);
        const std::vector<MDagPath> exportRoots = CollectExportRoots(mode);
        AppendDebugLog("writer: collected roots");
        if (exportRoots.empty())
        {
            AppendDebugLog("writer: no roots");
            return maya_dmx::ReportError("maya_dmx: nothing to export.");
        }

        DmxTextBuilder builder;
        ExportContext context;
        context.exportSkin = exportOptions.exportSkin;
        context.exportDeltaStates = exportOptions.exportDeltaStates;
        context.exportMetadata = exportOptions.exportMetadata;
        context.materialRoot = exportOptions.materialRoot;
        DmxElement *modelElement = builder.CreateElement("DmeModel");
        modelElement->attributes.push_back(MakeScalarAttribute("name", "string", "maya_export"));
        modelElement->attributes.push_back(MakeScalarAttribute("upAxis", "string", exportOptions.upAxis));
        if (exportOptions.exportMetadata && !exportOptions.materialRoot.empty())
        {
            modelElement->attributes.push_back(MakeScalarAttribute("mayaMaterialRoot", "string", exportOptions.materialRoot));
        }

        std::vector<DmxElement *> rootChildren;
        for (const MDagPath &rootPath : exportRoots)
        {
            RegisterDagElementsRecursive(builder, rootPath, context);
        }
        for (const MDagPath &rootPath : exportRoots)
        {
            if (DmxElement *child = BuildDagElement(builder, rootPath, context))
            {
                rootChildren.push_back(child);
            }
        }
        AppendDebugLog("writer: built dag elements");

        if (!rootChildren.empty())
        {
            modelElement->attributes.push_back(MakeElementArrayAttribute("children", rootChildren));
        }
        if (!context.jointElements.empty())
        {
            modelElement->attributes.push_back(MakeElementArrayAttribute("jointList", context.jointElements));
        }
        if (DmxElement *animationListElement = BuildAnimationListElement(builder, exportRoots, context))
        {
            modelElement->attributes.push_back(MakeInlineElementAttribute("animationList", animationListElement));
        }

        const bool binaryExport = exportOptions.binary;

        std::string serialized;
        std::string serializeError;
        if (binaryExport)
        {
            const std::string textContent = builder.Serialize(*modelElement);
            simple_dmx::Document document;
            std::string parseError;
            if (!document.Parse(textContent, parseError))
            {
                return maya_dmx::ReportError(("maya_dmx: internal error preparing binary export: " + parseError).c_str());
            }
            if (!simple_dmx::SerializeDocumentBinary(document, serialized, serializeError))
            {
                AppendDebugLog("writer: binary serialize failed");
                return maya_dmx::ReportError(serializeError.c_str());
            }
        }
        else
        {
            serialized = builder.Serialize(*modelElement);
        }
        AppendDebugLog("writer: serialized");

        std::ofstream output(fileObject.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to open output file ") + fileObject.rawFullName());
        }

        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.close();
        AppendDebugLog("writer: wrote file");

        if (!output)
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to write output file ") + fileObject.rawFullName());
        }

        return maya_dmx::ReportInfo(
            MString(binaryExport ? "maya_dmx: exported binary DMX to " : "maya_dmx: exported text DMX to ") + fileObject.rawFullName());
    }
    catch (const std::exception &exception)
    {
        AppendDebugLog("writer: std exception");
        return maya_dmx::ReportError(MString("maya_dmx: export failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        AppendDebugLog("writer: unknown exception");
        return maya_dmx::ReportError("maya_dmx: export failed with an unknown host exception.");
    }
}
