#include "SmdAnimationImporter.h"
#include "SmdImportUtils.h"

#include <common/AnimationCurveUtils.h>
#include <common/MayaCommandUtils.h>
#include <common/TransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <vector>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MTime.h>

SmdAnimationImporter::SmdAnimationImporter(
    std::shared_ptr<const simple_smd::Document> document,
    const SmdImportOptions &importOptions,
    const dcc_import_policy::SceneMergeResolver &mergeResolver,
    const std::unordered_map<int, MDagPath> &jointPathsByBone,
    const std::unordered_set<int> &reusedBoneIndices)
    : document_(document)
    , importOptions_(importOptions)
    , mergeResolver_(mergeResolver)
    , jointPathsByBone_(jointPathsByBone)
    , reusedBoneIndices_(reusedBoneIndices)
    , sourceDeltaProcessor_(importOptions, mergeResolver)
{
}

MStatus SmdAnimationImporter::Apply()
{
    if (document_->skeletonFrames.size() <= 1)
    {
        return MS::kSuccess;
    }

    for (const simple_smd::Node &node : document_->nodes)
    {
        if (mergeResolver_.usesAppendMissingObjects() &&
            reusedBoneIndices_.find(node.index) != reusedBoneIndices_.end())
        {
            continue;
        }

        const auto pathIt = jointPathsByBone_.find(node.index);
        if (pathIt == jointPathsByBone_.end())
        {
            continue;
        }

        MStatus status;
        MFnDependencyNode dependencyNodeFn(pathIt->second.node(), &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access dependency node for animated joint ") + node.name.c_str(), status);
        }

        std::vector<double> times;
        std::vector<double> txValues;
        std::vector<double> tyValues;
        std::vector<double> tzValues;
        std::vector<double> rxValues;
        std::vector<double> ryValues;
        std::vector<double> rzValues;
        std::vector<MVector> sampledTranslations;
        std::vector<MQuaternion> sampledRotations;
        const bool applyTopLevelCorrection =
            isTopLevelNode(node) && !importOptions_.transformCorrection.IsIdentity();
        const MQuaternion correctionRotation = importOptions_.transformCorrection.RotationQuaternion();

        for (const simple_smd::SkeletonFrame &frame : document_->skeletonFrames)
        {
            const simple_smd::SkeletonPose *pose = findPose(frame, node.index);
            if (!pose)
            {
                continue;
            }

            MVector correctedTranslation(pose->tx, pose->ty, pose->tz);
            if (applyTopLevelCorrection)
            {
                correctedTranslation = dcc_import_transform::ApplyToPoint(importOptions_.transformCorrection, correctedTranslation);
            }

            MQuaternion correctedRotation = MEulerRotation(pose->rx, pose->ry, pose->rz).asQuaternion();
            if (applyTopLevelCorrection)
            {
                correctedRotation = correctionRotation * correctedRotation;
            }

            times.push_back(smd_import_impl::FrameIndexToUiTimeValue(
                static_cast<double>(frame.time),
                importOptions_.animationFps));
            sampledTranslations.push_back(correctedTranslation);
            sampledRotations.push_back(correctedRotation);
        }

        if (times.size() <= 1)
        {
            continue;
        }

        status = sourceDeltaProcessor_.ApplyToSamples(pathIt->second, times, sampledTranslations, sampledRotations);
        if (!status)
        {
            return MS::kFailure;
        }

        MFnTransform transformFn(pathIt->second, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access transform for animated joint ") + node.name.c_str(), status);
        }

        MEulerRotation currentEulerRotation;
        status = transformFn.getRotation(currentEulerRotation);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to read current rotation for animated joint ") + node.name.c_str(), status);
        }

        for (size_t valueIndex = 0; valueIndex < sampledTranslations.size(); ++valueIndex)
        {
            const MVector finalTranslation = sampledTranslations[valueIndex];
            txValues.push_back(finalTranslation.x);
            tyValues.push_back(finalTranslation.y);
            tzValues.push_back(finalTranslation.z);

            const MQuaternion finalRotation = sampledRotations[valueIndex];
            MEulerRotation finalEuler = finalRotation.asEulerRotation();
            finalEuler.reorderIt(currentEulerRotation.order);
            rxValues.push_back(finalEuler.x);
            ryValues.push_back(finalEuler.y);
            rzValues.push_back(finalEuler.z);
        }

        const MPlug translateXPlug = dependencyNodeFn.findPlug("translateX", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateX plug for animated joint.", status);
        }

        MString layerName;
        const bool useLayer = usesAnimationLayerForTransforms();
        if (useLayer)
        {
            status = ensureTransformAnimationLayer(layerName);
            if (!status)
            {
                return MS::kFailure;
            }
        }

        status = useLayer ? setLayerCurveKeys(
            layerName,
            translateXPlug,
            times,
            txValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(translateXPlug, times, txValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write translateX animation.", status);
        }

        const MPlug translateYPlug = dependencyNodeFn.findPlug("translateY", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateY plug for animated joint.", status);
        }
        status = useLayer ? setLayerCurveKeys(
            layerName,
            translateYPlug,
            times,
            tyValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(translateYPlug, times, tyValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write translateY animation.", status);
        }

        const MPlug translateZPlug = dependencyNodeFn.findPlug("translateZ", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateZ plug for animated joint.", status);
        }
        status = useLayer ? setLayerCurveKeys(
            layerName,
            translateZPlug,
            times,
            tzValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(translateZPlug, times, tzValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write translateZ animation.", status);
        }

        const MPlug rotateXPlug = dependencyNodeFn.findPlug("rotateX", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateX plug for animated joint.", status);
        }
        status = useLayer ? setLayerCurveKeys(
            layerName,
            rotateXPlug,
            times,
            rxValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(rotateXPlug, times, rxValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write rotateX animation.", status);
        }

        const MPlug rotateYPlug = dependencyNodeFn.findPlug("rotateY", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateY plug for animated joint.", status);
        }
        status = useLayer ? setLayerCurveKeys(
            layerName,
            rotateYPlug,
            times,
            ryValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(rotateYPlug, times, ryValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write rotateY animation.", status);
        }

        const MPlug rotateZPlug = dependencyNodeFn.findPlug("rotateZ", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateZ plug for animated joint.", status);
        }
        status = useLayer ? setLayerCurveKeys(
            layerName,
            rotateZPlug,
            times,
            rzValues,
            mergeResolver_.usesSourceDeltaImport()) : setCurveKeys(rotateZPlug, times, rzValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to write rotateZ animation.", status);
        }
    }

    const double startTime = smd_import_impl::FrameIndexToUiTimeValue(
        static_cast<double>(document_->skeletonFrames.front().time),
        importOptions_.animationFps);
    const double endTime = smd_import_impl::FrameIndexToUiTimeValue(
        static_cast<double>(document_->skeletonFrames.back().time),
        importOptions_.animationFps);
    MAnimControl::setMinTime(MTime(startTime, MTime::uiUnit()));
    MAnimControl::setMaxTime(MTime(endTime, MTime::uiUnit()));
    MAnimControl::setCurrentTime(MTime(startTime, MTime::uiUnit()));
    return MS::kSuccess;
}

const simple_smd::SkeletonPose *SmdAnimationImporter::findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const
{
    const auto poseIt = std::find_if(
        frame.poses.begin(),
        frame.poses.end(),
        [boneIndex](const simple_smd::SkeletonPose &pose)
        {
            return pose.boneIndex == boneIndex;
        });
    return poseIt == frame.poses.end() ? nullptr : &(*poseIt);
}

bool SmdAnimationImporter::isTopLevelNode(const simple_smd::Node &node) const
{
    return jointPathsByBone_.find(node.parentIndex) == jointPathsByBone_.end();
}

bool SmdAnimationImporter::usesAnimationLayerForTransforms() const
{
    return mergeResolver_.usesAnimationLayerImport();
}

MStatus SmdAnimationImporter::ensureTransformAnimationLayer(MString &layerName) const
{
    const std::string configuredName = importOptions_.scenePolicy.animationLayerName.empty() ?
        std::string("smd_layer") :
        importOptions_.scenePolicy.animationLayerName;
    MStatus status = dcc_animation::EnsureAnimationLayerCached(
        configuredName.c_str(),
        importOptions_.scenePolicy.animationImportMode == dcc_import_policy::AnimationImportMode::ReplaceLayer,
        dcc_import_policy::UsesSourceDeltaImport(importOptions_.scenePolicy),
        true,
        transformAnimationLayerCache_,
        &layerName);
    if (!status)
    {
        return MS::kFailure;
    }

    return MS::kSuccess;
}

MStatus SmdAnimationImporter::setCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType) const
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    return dcc_animation::SetCurveKeys(plug, times, values, curveType, MTime::uiUnit());
}

MStatus SmdAnimationImporter::setLayerCurveKeys(
    const MString &layerName,
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    bool keepAdditiveMode) const
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    return dcc_animation::SetCurveKeysOnAnimationLayer(
        plug,
        times,
        values,
        layerName,
        false,
        keepAdditiveMode);
}
