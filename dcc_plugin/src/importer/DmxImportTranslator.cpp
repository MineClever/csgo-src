#include "DmxImportTranslator.h"
#include "DmxImportTranslatorTypes.h"
#include "DmxImportUtils.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxDocument.h"
#include "../common/SimpleDmxText.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MEulerRotation.h>
#include <maya/MQuaternion.h>
#include <maya/MFileObject.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include "DmxImportAnimation.h"
#include "DmxImportDag.h"
#include "DmxImportInternals.h"

namespace dmx_import_impl
{
using simple_dmx::FindAttributeElement;
using simple_dmx::FindAttributeElementArray;
using simple_dmx::FindAttributeString;
using simple_dmx::FindAttributeStringArray;
using simple_dmx::ParseNumberList;
using dmx_import_utils::SanitizeNodeName;

using dmx_import_translator::BlendShapeTargetBinding;
using dmx_import_translator::ImportContext;
using dmx_import_translator::ImportOptions;
using dmx_import_translator::ScalarAttributeBinding;

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

ImportOptions ParseImportOptions(const MString &options)
{
    ImportOptions importOptions;
    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    importOptions.importSkin = ParseBoolOption(optionMap, "importskin", true);
    importOptions.importMaterials = ParseBoolOption(optionMap, "importmaterials", true);
    importOptions.importDeltaStates = ParseBoolOption(optionMap, "importdeltastates", true);
    importOptions.applyAxisCorrection = ParseBoolOption(optionMap, "applyaxiscorrection", true);
    return importOptions;
}

std::string ReadTextFile(const MFileObject &fileObject)
{
    std::ifstream file(fileObject.rawFullName().asChar(), std::ios::in | std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::string NormalizeAxisName(std::string axisName)
{
    std::transform(axisName.begin(), axisName.end(), axisName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return axisName;
}

bool ComputeRootAxisCorrection(const std::string &sourceUpAxis, MEulerRotation &outRotation, MString &outWarning)
{
    const std::string normalizedSourceUpAxis = NormalizeAxisName(sourceUpAxis);
    if (normalizedSourceUpAxis.empty())
    {
        return false;
    }

    MStatus status;
    const bool mayaYAxisUp = MGlobal::isYAxisUp(&status);
    if (!status)
    {
        return false;
    }

    const bool mayaZAxisUp = MGlobal::isZAxisUp(&status);
    if (!status)
    {
        return false;
    }

    if (normalizedSourceUpAxis == "Z" && mayaYAxisUp)
    {
        outRotation = MEulerRotation(-1.57079632679, 0.0, 0.0);
        outWarning = "maya_dmx: imported Z-up model into a Y-up Maya scene with a -90deg X correction group.";
        return true;
    }

    if (normalizedSourceUpAxis == "Y" && mayaZAxisUp)
    {
        outRotation = MEulerRotation(1.57079632679, 0.0, 0.0);
        outWarning = "maya_dmx: imported Y-up model into a Z-up Maya scene with a +90deg X correction group.";
        return true;
    }

    return false;
}
} // namespace dmx_import_impl

using namespace dmx_import_impl;

void *DmxImportTranslator::Create()
{
    return new DmxImportTranslator();
}

bool DmxImportTranslator::haveReadMethod() const
{
    return true;
}

bool DmxImportTranslator::haveWriteMethod() const
{
    return false;
}

bool DmxImportTranslator::canBeOpened() const
{
    return true;
}

MString DmxImportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxImportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxImportTranslator::reader(const MFileObject &fileObject, const MString &options, FileAccessMode)
{
    const std::string fileText = ReadTextFile(fileObject);
    if (fileText.empty())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to read file ") + fileObject.rawFullName());
    }

    simple_dmx::Document document;
    std::string parseError;
    if (!simple_dmx::ParseDocument(fileText, document, parseError))
    {
        return maya_dmx::ReportError(MString("maya_dmx: parse error: ") + parseError.c_str());
    }

    const simple_dmx::Element *importRoot = FindImportRoot(document);
    if (!importRoot)
    {
        return maya_dmx::ReportError("maya_dmx: no importable DMX root element found.");
    }

    const ImportOptions importOptions = ParseImportOptions(options);

    ImportContext context{document};
    context.modelRoot = importRoot->type == "DmeModel" ? importRoot : nullptr;
    context.importSkin = importOptions.importSkin;
    context.importMaterials = importOptions.importMaterials;
    context.importDeltaStates = importOptions.importDeltaStates;
    context.applyAxisCorrection = importOptions.applyAxisCorrection;
    if (context.modelRoot)
    {
        CollectJointInfo(document, context.modelRoot, context);
    }

    MStatus status;
    MFnTransform rootTransformFn;
    MObject sceneRoot = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }

    rootTransformFn.setName(importRoot->name.empty() ? "dmx_import" : importRoot->name.c_str());

    const std::string upAxis = FindAttributeString(importRoot, "upAxis");
    MEulerRotation rootAxisCorrection;
    MString rootAxisWarning;
    const bool needsAxisCorrection =
        context.applyAxisCorrection &&
        ComputeRootAxisCorrection(upAxis, rootAxisCorrection, rootAxisWarning);

    status = ApplyTransform(document, importRoot, sceneRoot);
    if (!status)
    {
        return status;
    }

    if (needsAxisCorrection)
    {
        status = rootTransformFn.setRotation(rootAxisCorrection.asQuaternion());
        if (!status)
        {
            return status;
        }
        maya_dmx::ReportWarning(rootAxisWarning);
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document, importRoot, "children"))
    {
        status = ImportDagHierarchyRecursive(context, child, sceneRoot);
        if (!status)
        {
            return status;
        }
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document, importRoot, "children"))
    {
        status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            return status;
        }
    }

    const simple_dmx::Element *combinationOperator = FindCombinationOperator(document, document.GetRoot(), importRoot, context.modelRoot);
    status = CreateCombinationControls(context, combinationOperator, sceneRoot);
    if (!status)
    {
        return status;
    }

    const simple_dmx::Element *animationList = FindAnimationList(document, document.GetRoot(), importRoot, context.modelRoot);
    if (animationList)
    {
        const std::vector<const simple_dmx::Element *> animations = FindAttributeElementArray(document, animationList, "animations");
        for (const simple_dmx::Element *animation : animations)
        {
            status = ApplyChannelsClipAnimation(context, animation);
            if (!status)
            {
                return status;
            }
        }
    }

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + fileObject.rawFullName());
}
