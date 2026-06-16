#include "ObjImportSession.h"
#include "ObjSceneImporter.h"

#include <common/SceneMergeStrategy.h>
#include <common_obj/MayaObjCommon.h>
#include <common_obj/SimpleObjDocument.h>

#include <memory>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace obj_import_session_detail
{

struct ObjDocumentNormalizer
{
    static void NormalizeDocumentForImportCorrection(
        simple_obj::Document &document,
        const dcc_import_transform::TransformCorrection &correction)
    {
        if (correction.IsIdentity())
        {
            return;
        }

        rapidobj::Result &result = document.GetResult();
        rapidobj::Array<float> &positions = result.attributes.positions;

        for (size_t i = 0; i < positions.size(); i += 3)
        {
            const MVector correctedPosition = dcc_import_transform::ApplyToPoint(
                correction,
                MVector(positions[i + 0], positions[i + 1], positions[i + 2]));
            positions[i + 0] = static_cast<float>(correctedPosition.x);
            positions[i + 1] = static_cast<float>(correctedPosition.y);
            positions[i + 2] = static_cast<float>(correctedPosition.z);
        }

        rapidobj::Array<float> &normals = result.attributes.normals;

        for (size_t i = 0; i < normals.size(); i += 3)
        {
            const MVector correctedNormal = dcc_import_transform::ApplyToNormal(
                correction,
                MVector(normals[i + 0], normals[i + 1], normals[i + 2]));
            normals[i + 0] = static_cast<float>(correctedNormal.x);
            normals[i + 1] = static_cast<float>(correctedNormal.y);
            normals[i + 2] = static_cast<float>(correctedNormal.z);
        }
    }
};

} // namespace obj_import_session_detail

ObjImportSession::ObjImportSession(const MFileObject &fileObject, const MString &options)
    : fileObject_(fileObject), options_(options)
{
}

MStatus ObjImportSession::Run()
{
    const MStatus validationStatus = validateInputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    auto document = std::make_shared<simple_obj::Document>();
    std::string errorMessage;
    if (!document->ParseFromFile(fileObject_.resolvedFullName().asChar(), &errorMessage))
    {
        return maya_obj::ReportError(
            MString("maya_obj: failed to parse OBJ file: ") + errorMessage.c_str());
    }

    const rapidobj::Result &result = document->GetResult();

    if (result.shapes.empty())
    {
        return maya_obj::ReportError(
            MString("maya_obj: OBJ file did not contain any shapes: ") + fileObject_.rawFullName());
    }

    const ObjImportOptions importOptions = parseOptions();

    // Apply transform correction to the raw position / normal data before scene import
    obj_import_session_detail::ObjDocumentNormalizer::NormalizeDocumentForImportCorrection(
        *document, importOptions.transformCorrection);

    // Use identity correction from here on since we already baked it into the data
    ObjImportOptions normalizedImportOptions = importOptions;
    normalizedImportOptions.transformCorrection = dcc_import_transform::TransformCorrection();

    dcc_import_policy::SceneMergeStrategy sceneMergeStrategy(normalizedImportOptions.scenePolicy);
    normalizedImportOptions.scenePolicy = sceneMergeStrategy.policy();

    ObjSceneImporter importer(document, normalizedImportOptions);
    return importer.Import();
}

MStatus ObjImportSession::validateInputFile() const
{
    if (!maya_obj::HasObjExtension(fileObject_))
    {
        return maya_obj::ReportError(
            MString("maya_obj: unsupported import extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

ObjImportOptions ObjImportSession::parseOptions() const
{
    ObjImportOptions parsedOptions;

    if (options_.length() == 0)
    {
        return parsedOptions;
    }

    const std::unordered_map<std::string, std::string> optionMap =
        dcc_import_policy::ParseOptionMap(options_);

    dcc_import_policy::SceneMergeStrategy sceneMergeStrategy =
        dcc_import_policy::SceneMergeStrategy::Parse(optionMap);
    sceneMergeStrategy.captureCurrentNamespace();
    sceneMergeStrategy.normalizeForImport(fileObject_.rawName().asChar());
    parsedOptions.scenePolicy = sceneMergeStrategy.policy();

    parsedOptions.transformCorrection = dcc_import_transform::ParseTransformCorrection(optionMap);

    auto flipUvVIt = optionMap.find("flipuvv");
    if (flipUvVIt != optionMap.end())
    {
        parsedOptions.flipUvV = dcc_import_policy::ParseBoolOption(optionMap, "flipuvv", true);
    }

    return parsedOptions;
}
