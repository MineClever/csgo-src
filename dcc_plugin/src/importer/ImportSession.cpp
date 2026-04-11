#include "ImportSession.h"

#include "DmxImportAnimation.h"
#include "DmxImportDag.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxText.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MEulerRotation.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MQuaternion.h>

namespace
{
using simple_dmx::FindAttributeElementArray;
using simple_dmx::FindAttributeString;
using dmx_import_translator::ImportContext;
using dmx_import_translator::ImportOptions;
using namespace dmx_import_impl;

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

std::string ReadTextFile(const MString &filePath)
{
    std::ifstream file(filePath.asChar(), std::ios::in | std::ios::binary);
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
} // namespace

ImportSession::ImportSession(const MFileObject &fileObject, const MString &options)
    : filePath_(fileObject.rawFullName())
    , optionsText_(options)
{
}

MStatus ImportSession::Run()
{
    MStatus status = LoadDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    ImportContext context{document_};
    context.modelRoot = importRoot_->type == "DmeModel" ? importRoot_ : nullptr;
    context.importSkin = importOptions_.importSkin;
    context.importMaterials = importOptions_.importMaterials;
    context.importDeltaStates = importOptions_.importDeltaStates;
    context.applyAxisCorrection = importOptions_.applyAxisCorrection;
    if (context.modelRoot)
    {
        CollectJointInfo(document_, context.modelRoot, context);
    }

    MObject sceneRoot;
    status = CreateSceneRoot(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = ImportHierarchy(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = ImportAnimation(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + filePath_);
}

MStatus ImportSession::LoadDocument()
{
    const std::string fileText = ReadTextFile(filePath_);
    if (fileText.empty())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to read file ") + filePath_);
    }

    std::string parseError;
    if (!simple_dmx::ParseDocument(fileText, document_, parseError))
    {
        return maya_dmx::ReportError(MString("maya_dmx: parse error: ") + parseError.c_str());
    }

    importRoot_ = FindImportRoot(document_);
    if (!importRoot_)
    {
        return maya_dmx::ReportError("maya_dmx: no importable DMX root element found.");
    }

    importOptions_ = ParseImportOptions(optionsText_);
    return MStatus::kSuccess;
}

MStatus ImportSession::CreateSceneRoot(ImportContext &context, MObject &sceneRoot) const
{
    MStatus status;
    MFnTransform rootTransformFn;
    sceneRoot = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    rootTransformFn.setName(importRoot_->name.empty() ? "dmx_import" : importRoot_->name.c_str());

    const std::string upAxis = FindAttributeString(importRoot_, "upAxis");
    MEulerRotation rootAxisCorrection;
    MString rootAxisWarning;
    const bool needsAxisCorrection =
        context.applyAxisCorrection &&
        ComputeRootAxisCorrection(upAxis, rootAxisCorrection, rootAxisWarning);

    status = ApplyTransform(document_, importRoot_, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (needsAxisCorrection)
    {
        status = rootTransformFn.setRotation(rootAxisCorrection.asQuaternion());
        if (!status)
        {
            return MStatus::kFailure;
        }
        maya_dmx::ReportWarning(rootAxisWarning);
    }

    return MStatus::kSuccess;
}

MStatus ImportSession::ImportHierarchy(ImportContext &context, MObject sceneRoot) const
{
    for (const simple_dmx::Element *child : FindAttributeElementArray(document_, importRoot_, "children"))
    {
        MStatus status = ImportDagHierarchyRecursive(context, child, sceneRoot);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document_, importRoot_, "children"))
    {
        MStatus status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MStatus::kSuccess;
}

MStatus ImportSession::ImportAnimation(ImportContext &context, MObject sceneRoot) const
{
    const simple_dmx::Element *combinationOperator =
        FindCombinationOperator(document_, document_.GetRoot(), importRoot_, context.modelRoot);
    MStatus status = CreateCombinationControls(context, combinationOperator, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const simple_dmx::Element *animationList =
        FindAnimationList(document_, document_.GetRoot(), importRoot_, context.modelRoot);
    if (!animationList)
    {
        return MStatus::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> animations =
        FindAttributeElementArray(document_, animationList, "animations");
    for (const simple_dmx::Element *animation : animations)
    {
        status = ApplyChannelsClipAnimation(context, animation);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MStatus::kSuccess;
}
