#include "GltfExportSession.h"
#include "GltfSceneExporter.h"

#include <common_gltf/MayaGltfCommon.h>

#include <common/ImportPolicy.h>

#include <unordered_map>

GltfExportSession::GltfExportSession(
    const MFileObject &fileObject,
    const MString &options,
    MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , options_(options)
    , mode_(mode)
{
}

MStatus GltfExportSession::Run()
{
    MStatus validationStatus = validateOutputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    const GltfExportOptions exportOptions = parseOptions();

    GltfSceneExporter exporter(fileObject_, exportOptions, mode_);
    return exporter.Export();
}

MStatus GltfExportSession::validateOutputFile() const
{
    if (!maya_gltf::HasGltfOrGlbExtension(fileObject_))
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: unsupported export extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

GltfExportOptions GltfExportSession::parseOptions() const
{
    GltfExportOptions parsedOptions;

    if (options_.length() == 0)
    {
        return parsedOptions;
    }

    const std::unordered_map<std::string, std::string> optionMap =
        dcc_import_policy::ParseOptionMap(options_);

    // Data to export (keys are lowercase; ParseOptionMap lowercases all keys)
    parsedOptions.exportMesh = dcc_import_policy::ParseBoolOption(optionMap, "exportmesh", true);
    parsedOptions.exportAnimation = dcc_import_policy::ParseBoolOption(optionMap, "exportanimation", true);
    parsedOptions.exportCamera = dcc_import_policy::ParseBoolOption(optionMap, "exportcamera", false);
    parsedOptions.exportSkin = dcc_import_policy::ParseBoolOption(optionMap, "exportskin", true);
    parsedOptions.exportMaterial = dcc_import_policy::ParseBoolOption(optionMap, "exportmaterial", true);
    parsedOptions.exportVertexColors = dcc_import_policy::ParseBoolOption(optionMap, "exportvertexcolors", true);
    parsedOptions.exportNormals = dcc_import_policy::ParseBoolOption(optionMap, "exportnormals", true);
    parsedOptions.exportUVs = dcc_import_policy::ParseBoolOption(optionMap, "exportuvs", true);
    parsedOptions.flipUvV = dcc_import_policy::ParseBoolOption(optionMap, "flipuvv", true);

    // Transform offset options (lowercase; ParseOptionMap lowercases all keys)
    auto it = optionMap.find("translatex");
    if (it != optionMap.end()) parsedOptions.translateX = std::stof(it->second);
    it = optionMap.find("translatey");
    if (it != optionMap.end()) parsedOptions.translateY = std::stof(it->second);
    it = optionMap.find("translatez");
    if (it != optionMap.end()) parsedOptions.translateZ = std::stof(it->second);
    it = optionMap.find("rotatex");
    if (it != optionMap.end()) parsedOptions.rotateX = std::stof(it->second);
    it = optionMap.find("rotatey");
    if (it != optionMap.end()) parsedOptions.rotateY = std::stof(it->second);
    it = optionMap.find("rotatez");
    if (it != optionMap.end()) parsedOptions.rotateZ = std::stof(it->second);
    it = optionMap.find("scalex");
    if (it != optionMap.end()) parsedOptions.scaleX = std::stof(it->second);
    it = optionMap.find("scaley");
    if (it != optionMap.end()) parsedOptions.scaleY = std::stof(it->second);
    it = optionMap.find("scalez");
    if (it != optionMap.end()) parsedOptions.scaleZ = std::stof(it->second);

    return parsedOptions;
}
