#include "SmdSourceDeltaProcessor.h"

#include "SmdImportUtils.h"

#include <common/AnimationSampleUtils.h>
#include <common/MayaCommandUtils.h>
#include <common/SourceDeltaUtils.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <vector>

#include <maya/MQuaternion.h>
#include <maya/MVector.h>

SmdSourceDeltaProcessor::SmdSourceDeltaProcessor(
    const SmdImportOptions &importOptions,
    const dcc_import_policy::SceneMergeResolver &mergeResolver)
    : importOptions_(importOptions)
    , mergeResolver_(mergeResolver)
{
}

MStatus SmdSourceDeltaProcessor::ApplyToSamples(
    const MDagPath &jointPath,
    const std::vector<double> &times,
    std::vector<MVector> &translations,
    std::vector<MQuaternion> &rotations) const
{
    const dcc_import_policy::SourceDeltaMode mode = importOptions_.scenePolicy.sourceDeltaMode;
    if (mode == dcc_import_policy::SourceDeltaMode::None)
    {
        return MS::kSuccess;
    }

    if (translations.empty() || rotations.empty() || translations.size() != rotations.size())
    {
        return MS::kSuccess;
    }

    if (mergeResolver_.usesAnimationLayerImport() &&
        (mode == dcc_import_policy::SourceDeltaMode::Subtract ||
         mode == dcc_import_policy::SourceDeltaMode::PreSubtract))
    {
        return MS::kSuccess;
    }

    const bool useSceneClip = importOptions_.scenePolicy.sourceDeltaUseClip &&
        !dcc_animation::IsEmptyAnimationLayerName(importOptions_.scenePolicy.sourceDeltaClip.c_str());

    if (mode == dcc_import_policy::SourceDeltaMode::LinearDelta ||
        mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
    {
        dcc_source_delta::ApplySourceDeltaLinearReferenceSamples(translations, mode);
        dcc_source_delta::ApplySourceDeltaLinearReferenceSamples(rotations, mode);
        return MS::kSuccess;
    }

    if (useSceneClip &&
        (mode == dcc_import_policy::SourceDeltaMode::Subtract ||
         mode == dcc_import_policy::SourceDeltaMode::PreSubtract))
    {
        std::vector<MVector> referenceTranslations;
        std::vector<MQuaternion> referenceRotations;
        MStatus status = dcc_animation::BuildSceneLayerTranslationSamples(
            importOptions_.scenePolicy.sourceDeltaClip.c_str(),
            jointPath,
            times,
            MTime::uiUnit(),
            referenceTranslations);
        if (status)
        {
            status = dcc_animation::BuildSceneLayerQuaternionSamples(
                importOptions_.scenePolicy.sourceDeltaClip.c_str(),
                jointPath,
                times,
                MTime::uiUnit(),
                referenceRotations);
        }
        if (!status || referenceTranslations.size() != translations.size() || referenceRotations.size() != rotations.size())
        {
            return maya_smd::ReportError("maya_smd: sourceDelta scene layer samples were missing.");
        }

        const size_t referenceIndex = std::min(
            static_cast<size_t>(importOptions_.scenePolicy.sourceDeltaReferenceFrame),
            referenceTranslations.size() - 1);
        dcc_source_delta::ApplySourceDeltaReferenceValue(translations, referenceTranslations[referenceIndex], mode);
        dcc_source_delta::ApplySourceDeltaReferenceValue(rotations, referenceRotations[referenceIndex], mode);
        return MS::kSuccess;
    }

    if (mode == dcc_import_policy::SourceDeltaMode::Subtract ||
        mode == dcc_import_policy::SourceDeltaMode::PreSubtract)
    {
        std::vector<MVector> referenceTranslations;
        std::vector<MQuaternion> referenceRotations;
        MStatus status = dcc_animation::BuildSceneReferenceTranslationSamples(
            jointPath,
            times,
            MTime::uiUnit(),
            referenceTranslations);
        if (status)
        {
            status = dcc_animation::BuildSceneReferenceQuaternionSamples(
                jointPath,
                times,
                MTime::uiUnit(),
                referenceRotations);
        }
        if (!status || referenceTranslations.size() != translations.size() || referenceRotations.size() != rotations.size())
        {
            return maya_smd::ReportError("maya_smd: sourceDelta reference scene samples were missing.");
        }

        dcc_source_delta::ApplySourceDeltaReferenceSamples(translations, referenceTranslations, mode);
        dcc_source_delta::ApplySourceDeltaReferenceSamples(rotations, referenceRotations, mode);
        return MS::kSuccess;
    }

    return maya_smd::ReportError("maya_smd: sourceDelta currently only supports subtract, presubtract, lineardelta and splinedelta for SMD transform import.");
}
