#include "SmdSourceDeltaProcessor.h"

#include "SmdImportUtils.h"

#include <common/MayaCommandUtils.h>
#include <common/SourceDeltaUtils.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>
#include <maya/MTime.h>
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
        !smd_import_impl::IsEmptyLayerName(importOptions_.scenePolicy.sourceDeltaClip);

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
        MStatus status = buildSceneLayerSamples(
            importOptions_.scenePolicy.sourceDeltaClip.c_str(),
            jointPath,
            times,
            referenceTranslations,
            referenceRotations);
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
        MStatus status = buildSceneReferenceSamples(jointPath, times, referenceTranslations, referenceRotations);
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

MStatus SmdSourceDeltaProcessor::sampleLayerPlugValue(
    const MString &layerName,
    const MPlug &plug,
    double time,
    double &value) const
{
    value = 0.0;
    if (plug.isNull())
    {
        return MS::kFailure;
    }

    if (smd_import_impl::IsEmptyLayerName(layerName.asChar()))
    {
        smd_import_impl::CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(time, MTime::uiUnit()));
        value = plug.asDouble();
        return MS::kSuccess;
    }

    MStringArray curveNames;
    MStatus status = maya_cmd::FindAnimationLayerCurvesForPlug(layerName, plug, curveNames);
    if (!status)
    {
        return MS::kFailure;
    }

    if (curveNames.length() == 0)
    {
        smd_import_impl::CurrentTimeGuard currentTimeGuard;
        MAnimControl::setCurrentTime(MTime(time, MTime::uiUnit()));
        value = plug.asDouble();
        return MS::kSuccess;
    }

    MObject curveObject;
    const bool curveFound = maya_cmd::TryGetNodeByName(curveNames[0], curveObject);
    if (!curveFound || curveObject.isNull() || !curveObject.hasFn(MFn::kAnimCurve))
    {
        return MS::kFailure;
    }

    MFnAnimCurve curveFn(curveObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    value = curveFn.evaluate(MTime(time, MTime::uiUnit()));
    return MS::kSuccess;
}

MStatus SmdSourceDeltaProcessor::buildSceneReferenceSamples(
    const MDagPath &jointPath,
    const std::vector<double> &times,
    std::vector<MVector> &translations,
    std::vector<MQuaternion> &rotations) const
{
    translations.clear();
    rotations.clear();
    translations.reserve(times.size());
    rotations.reserve(times.size());
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode dependencyNodeFn(jointPath.node(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug translateXPlug = dependencyNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateYPlug = dependencyNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateZPlug = dependencyNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnTransform transformFn(jointPath, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    smd_import_impl::CurrentTimeGuard currentTimeGuard;
    for (double time : times)
    {
        MAnimControl::setCurrentTime(MTime(time, MTime::uiUnit()));
        translations.emplace_back(
            translateXPlug.asDouble(),
            translateYPlug.asDouble(),
            translateZPlug.asDouble());

        MEulerRotation currentEulerRotation;
        status = transformFn.getRotation(currentEulerRotation);
        if (!status)
        {
            return MS::kFailure;
        }
        rotations.push_back(currentEulerRotation.asQuaternion());
    }

    return MS::kSuccess;
}

MStatus SmdSourceDeltaProcessor::buildSceneLayerSamples(
    const MString &layerName,
    const MDagPath &jointPath,
    const std::vector<double> &times,
    std::vector<MVector> &translations,
    std::vector<MQuaternion> &rotations) const
{
    translations.clear();
    rotations.clear();
    if (times.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform transformFn(jointPath, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MEulerRotation currentEulerRotation;
    status = transformFn.getRotation(currentEulerRotation);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnDependencyNode dependencyNodeFn(jointPath.node(), &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug translateXPlug = dependencyNodeFn.findPlug("translateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateYPlug = dependencyNodeFn.findPlug("translateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug translateZPlug = dependencyNodeFn.findPlug("translateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug rotateXPlug = dependencyNodeFn.findPlug("rotateX", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug rotateYPlug = dependencyNodeFn.findPlug("rotateY", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }
    MPlug rotateZPlug = dependencyNodeFn.findPlug("rotateZ", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (double time : times)
    {
        double tx = 0.0;
        double ty = 0.0;
        double tz = 0.0;
        double rx = 0.0;
        double ry = 0.0;
        double rz = 0.0;
        status = sampleLayerPlugValue(layerName, translateXPlug, time, tx);
        if (!status)
        {
            return MS::kFailure;
        }
        status = sampleLayerPlugValue(layerName, translateYPlug, time, ty);
        if (!status)
        {
            return MS::kFailure;
        }
        status = sampleLayerPlugValue(layerName, translateZPlug, time, tz);
        if (!status)
        {
            return MS::kFailure;
        }
        status = sampleLayerPlugValue(layerName, rotateXPlug, time, rx);
        if (!status)
        {
            return MS::kFailure;
        }
        status = sampleLayerPlugValue(layerName, rotateYPlug, time, ry);
        if (!status)
        {
            return MS::kFailure;
        }
        status = sampleLayerPlugValue(layerName, rotateZPlug, time, rz);
        if (!status)
        {
            return MS::kFailure;
        }

        translations.emplace_back(tx, ty, tz);
        MEulerRotation eulerRotation(rx, ry, rz);
        eulerRotation.reorderIt(currentEulerRotation.order);
        rotations.push_back(eulerRotation.asQuaternion());
    }

    return MS::kSuccess;
}
