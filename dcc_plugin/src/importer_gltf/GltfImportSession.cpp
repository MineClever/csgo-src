#include "GltfImportSession.h"
#include "GltfSceneImporter.h"

#include <common/ImportPolicy.h>
#include <common_gltf/MayaGltfCommon.h>
#include <common_gltf/SimpleGltfDocument.h>
#include <common_gltf/cgltf.h>

#include <memory>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include <maya/MTime.h>

GltfImportSession::GltfImportSession(const MFileObject &fileObject, const MString &options)
    : fileObject_(fileObject), options_(options)
{
}

MStatus GltfImportSession::Run()
{
    MStatus validationStatus = validateInputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    auto document = std::make_shared<simple_gltf::Document>();
    std::string errorMessage;
    if (!document->ParseFromFile(fileObject_.resolvedFullName().asChar(), &errorMessage))
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: failed to parse glTF file: ") + errorMessage.c_str());
    }

    if (!document->IsValid())
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: parsed glTF document is null: ") + fileObject_.rawFullName());
    }

    // Quick validation: ensure at least one scene or node exists
    const cgltf_data *data = document->GetData();
    if (data->scenes_count == 0 && data->nodes_count == 0)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: glTF file contains no scenes or nodes: ") + fileObject_.rawFullName());
    }

    const GltfImportOptions importOptions = parseOptions();

    GltfSceneImporter importer(document, importOptions);
    return importer.Import();
}

MStatus GltfImportSession::validateInputFile() const
{
    if (!maya_gltf::HasGltfOrGlbExtension(fileObject_))
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: unsupported import extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

GltfImportOptions GltfImportSession::parseOptions() const
{
    GltfImportOptions parsedOptions;

    // Initialize frame rate from current Maya scene FPS (SMD-compatible default)
    {
        const double sceneFps = MTime(1.0, MTime::kSeconds).as(MTime::uiUnit());
        parsedOptions.frameRate = (sceneFps > 0.5f) ? static_cast<float>(sceneFps) : 30.0f;
    }

    if (options_.length() == 0)
    {
        return parsedOptions;
    }

    const std::unordered_map<std::string, std::string> optionMap =
        dcc_import_policy::ParseOptionMap(options_);

    // Data to import (keys are lowercase; ParseOptionMap lowercases all keys)
    parsedOptions.importMesh = dcc_import_policy::ParseBoolOption(optionMap, "importmesh", true);
    parsedOptions.importAnimation = dcc_import_policy::ParseBoolOption(optionMap, "importanimation", true);
    parsedOptions.importSkin = dcc_import_policy::ParseBoolOption(optionMap, "importskin", true);
    parsedOptions.importCamera = dcc_import_policy::ParseBoolOption(optionMap, "importcamera", true);
    parsedOptions.importMaterial = dcc_import_policy::ParseBoolOption(optionMap, "importmaterial", true);
    parsedOptions.importVertexColors = dcc_import_policy::ParseBoolOption(optionMap, "importvertexcolors", true);
    parsedOptions.importNormals = dcc_import_policy::ParseBoolOption(optionMap, "importnormals", true);
    parsedOptions.importUVs = dcc_import_policy::ParseBoolOption(optionMap, "importuvs", true);

    parsedOptions.useSceneRoot = dcc_import_policy::ParseBoolOption(optionMap, "usesceneroot", false);
    parsedOptions.restoreDefaultPoseAfterSkin =
        dcc_import_policy::ParseBoolOption(optionMap, "restoredefaultposeafterskin", true);
    parsedOptions.enableColorManagement =
        dcc_import_policy::ParseBoolOption(optionMap, "enablecolormanagement", true);

    // Frame rate options (override scene default with UI values if present)
    parsedOptions.detectFrameRate = dcc_import_policy::ParseBoolOption(optionMap, "detectframerate", true);
    {
        auto fit = optionMap.find("framerate");
        if (fit != optionMap.end())
        {
            const double parsedFps = std::atof(fit->second.c_str());
            if (parsedFps > 0.5)
                parsedOptions.frameRate = static_cast<float>(parsedFps);
        }
    }

    parsedOptions.transformCorrection = dcc_import_transform::ParseTransformCorrection(optionMap);

    return parsedOptions;
}
