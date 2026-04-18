#include "SmdImportSession.h"
#include "SmdSceneImporter.h"

#include <common_smd/MayaSmdCommon.h>
#include <common_smd/SimpleSmdDocument.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <maya/MEulerRotation.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

namespace
{
std::string SanitizeLayerName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("smd_layer") : value;
}

std::unordered_set<int> CollectTopLevelBoneIndices(const simple_smd::Document &document)
{
    std::unordered_set<int> knownBoneIndices;
    std::unordered_set<int> topLevelBoneIndices;
    knownBoneIndices.reserve(document.nodes.size());
    topLevelBoneIndices.reserve(document.nodes.size());

    for (const simple_smd::Node &node : document.nodes)
    {
        knownBoneIndices.insert(node.index);
    }

    for (const simple_smd::Node &node : document.nodes)
    {
        if (node.parentIndex < 0 || knownBoneIndices.find(node.parentIndex) == knownBoneIndices.end())
        {
            topLevelBoneIndices.insert(node.index);
        }
    }

    return topLevelBoneIndices;
}

void ApplyCorrectionToPose(
    simple_smd::SkeletonPose &pose,
    const dcc_import_transform::TransformCorrection &correction,
    bool isTopLevelBone)
{
    MVector correctedTranslation = dcc_import_transform::ApplyToTranslationScale(
        correction,
        MVector(pose.tx, pose.ty, pose.tz));
    MQuaternion correctedRotation = MEulerRotation(pose.rx, pose.ry, pose.rz).asQuaternion();
    if (isTopLevelBone)
    {
        correctedTranslation = dcc_import_transform::ApplyToTopLevelTranslation(
            correction,
            MVector(pose.tx, pose.ty, pose.tz));
        correctedRotation = dcc_import_transform::ApplyToQuaternion(correction, correctedRotation);
    }
    const MEulerRotation correctedEuler = correctedRotation.asEulerRotation();

    pose.tx = correctedTranslation.x;
    pose.ty = correctedTranslation.y;
    pose.tz = correctedTranslation.z;
    pose.rx = correctedEuler.x;
    pose.ry = correctedEuler.y;
    pose.rz = correctedEuler.z;
}

void NormalizeDocumentForImportCorrection(
    simple_smd::Document &document,
    const dcc_import_transform::TransformCorrection &correction)
{
    if (correction.IsIdentity())
    {
        return;
    }

    const std::unordered_set<int> topLevelBoneIndices = CollectTopLevelBoneIndices(document);
    for (simple_smd::SkeletonFrame &frame : document.skeletonFrames)
    {
        for (simple_smd::SkeletonPose &pose : frame.poses)
        {
            ApplyCorrectionToPose(
                pose,
                correction,
                topLevelBoneIndices.find(pose.boneIndex) != topLevelBoneIndices.end());
        }
    }

    for (simple_smd::Triangle &triangle : document.triangles)
    {
        for (simple_smd::TriangleVertex &vertex : triangle.vertices)
        {
            const MVector correctedPosition = dcc_import_transform::ApplyToPoint(
                correction,
                MVector(vertex.px, vertex.py, vertex.pz));
            const MVector correctedNormal = dcc_import_transform::ApplyToNormal(
                correction,
                MVector(vertex.nx, vertex.ny, vertex.nz));

            vertex.px = correctedPosition.x;
            vertex.py = correctedPosition.y;
            vertex.pz = correctedPosition.z;
            vertex.nx = correctedNormal.x;
            vertex.ny = correctedNormal.y;
            vertex.nz = correctedNormal.z;
        }
    }
}
}

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
    NormalizeDocumentForImportCorrection(*document, importOptions.transformCorrection);
    SmdImportOptions normalizedImportOptions = importOptions;
    normalizedImportOptions.transformCorrection = dcc_import_transform::TransformCorrection();
    if (dcc_import_policy::UsesAnimationOnlyImport(importOptions.scenePolicy) &&
        !dcc_import_policy::UsesSceneRoot(importOptions.scenePolicy))
    {
        normalizedImportOptions.scenePolicy.rootMode = dcc_import_policy::RootMode::SceneRoot;
        maya_smd::ReportWarning("maya_smd: importMode=animationOnly forces useSceneRoot=1 so imported animation can target existing scene joints.");
    }

    if (dcc_import_policy::UsesUpdateCurrentScene(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=update now reuses matching hierarchy, overwrites reused bind pose/base animation, and attempts in-place mesh/skin updates when matching nodes already exist; fine-grained scene-merge is still not implemented yet.");
    }
    else if (dcc_import_policy::UsesAppendMissingObjects(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=append currently reuses matching hierarchy and existing mesh groups, but full scene-merge behavior is not implemented yet.");
    }
    else if (dcc_import_policy::UsesAnimationOnlyImport(importOptions.scenePolicy))
    {
        maya_smd::ReportWarning("maya_smd: importMode=animationOnly only targets matching existing scene joints; missing joints and all mesh import work are skipped.");
    }

    if (importOptions.scenePolicy.importAnimationToLayer)
    {
        maya_smd::ReportWarning("maya_smd: importAnimationToLayer writes imported animation to a Maya override animation layer. Base animation remains unchanged while the layer is muted.");
    }

    SmdSceneImporter importer(document, normalizedImportOptions);
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
    if (parsedOptions.scenePolicy.importAnimationToLayer && parsedOptions.scenePolicy.animationLayerName.empty())
    {
        parsedOptions.scenePolicy.animationLayerName = SanitizeLayerName(fileObject_.rawName().asChar()) + "_layer";
    }
    parsedOptions.transformCorrection = dcc_import_transform::ParseTransformCorrection(optionMap);
    return parsedOptions;
}
