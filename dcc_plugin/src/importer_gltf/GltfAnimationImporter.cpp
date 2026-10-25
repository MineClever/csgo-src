#include "GltfAnimationImporter.h"
#include "GltfAccessorReader.h"
#include "GltfImportSession.h"  // GltfImportOptions

#include <common/AnimationCurveUtils.h>
#include <common/TransformCorrection.h>
#include <common_gltf/MayaGltfCommon.h>

#include <cmath>

#include <maya/MDagPath.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MEulerRotation.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

GltfAnimationImporter::GltfAnimationImporter(
    std::shared_ptr<const simple_gltf::Document> document,
    const std::unordered_map<const cgltf_node *, MDagPath> &importedNodePaths,
    const GltfImportOptions &importOptions)
    : document_(std::move(document))
    , importedNodePaths_(importedNodePaths)
    , importOptions_(importOptions)
{
}

MStatus GltfAnimationImporter::ImportAnimations(const cgltf_data *data)
{
    if (!data || data->animations_count == 0) return MS::kSuccess;

    MStatus status;

    for (cgltf_size a = 0; a < data->animations_count; ++a)
    {
        const cgltf_animation &anim = data->animations[a];

        for (cgltf_size c = 0; c < anim.channels_count; ++c)
        {
            const cgltf_animation_channel &channel = anim.channels[c];
            const cgltf_animation_sampler &sampler = *channel.sampler;

            if (!channel.target_node || !sampler.input || !sampler.output)
                continue;

            // Read time keys
            std::vector<float> floatTimes;
            {
                GltfAccessorReader inputReader(sampler.input);
                if (!inputReader.ReadFloats(floatTimes, 1))
                    continue;
            }
            std::vector<double> times(floatTimes.begin(), floatTimes.end());

            // Frame rate detection
            double effectiveFps = 0.0;
            if (!importOptions_.detectFrameRate)
            {
                effectiveFps = static_cast<double>(importOptions_.frameRate);
            }
            else if (times.size() >= 2)
            {
                const double duration = times.back() - times.front();
                if (duration > 0.0)
                {
                    const double detected = (times.size() - 1) / duration;
                    if (detected >= 1.0 && detected <= 1000.0)
                        effectiveFps = std::round(detected);
                }
            }
            const MTime::Unit timeUnit = (effectiveFps > 0.0) ? MTime::uiUnit() : MTime::kSeconds;
            if (effectiveFps > 0.0)
            {
                for (auto &t : times)
                    t *= effectiveFps;
            }

            // Determine accessor layout
            const cgltf_size numComponents = cgltf_num_components(sampler.output->type);
            const bool isCubicSpline = (sampler.interpolation == cgltf_interpolation_type_cubic_spline);
            const cgltf_size floatsPerKey = numComponents * (isCubicSpline ? 3 : 1);
            const cgltf_size valueOffset = isCubicSpline ? numComponents : 0;

            // Read output values
            std::vector<float> floatValues;
            {
                GltfAccessorReader outputReader(sampler.output);
                if (!outputReader.ReadFloats(floatValues, 1))
                    continue;
            }
            if (floatValues.size() < times.size() * floatsPerKey)
                continue;

            // Find Maya transform for target node
            const auto nodeIt = importedNodePaths_.find(channel.target_node);
            if (nodeIt == importedNodePaths_.end())
                continue;

            const MDagPath &targetPath = nodeIt->second;
            MFnDependencyNode depFn(targetPath.node());

            // Top-level transform correction
            const bool isTopLevel = channel.target_node->parent == nullptr;
            const bool applyTopLevelCorrection =
                isTopLevel && !importOptions_.transformCorrection.IsIdentity();
            const dcc_import_transform::TransformCorrection &correction = importOptions_.transformCorrection;

            switch (channel.target_path)
            {
            case cgltf_animation_path_type_rotation:
            {
                if (numComponents < 4) continue;

                MFnTransform transformFn(targetPath, &status);
                if (!status) continue;
                MEulerRotation currentEuler;
                status = transformFn.getRotation(currentEuler);
                if (!status) continue;

                const MQuaternion correctionQ = correction.RotationQuaternion();

                std::vector<double> rxValues, ryValues, rzValues;
                rxValues.reserve(times.size());
                ryValues.reserve(times.size());
                rzValues.reserve(times.size());

                for (size_t k = 0; k < times.size(); ++k)
                {
                    const cgltf_size base = k * floatsPerKey + valueOffset;
                    MQuaternion q(
                        static_cast<double>(floatValues[base + 0]),
                        static_cast<double>(floatValues[base + 1]),
                        static_cast<double>(floatValues[base + 2]),
                        static_cast<double>(floatValues[base + 3]));

                    if (applyTopLevelCorrection)
                        q = correctionQ * q;

                    MEulerRotation euler = q.asEulerRotation();
                    euler.reorderIt(currentEuler.order);
                    rxValues.push_back(euler.x);
                    ryValues.push_back(euler.y);
                    rzValues.push_back(euler.z);
                }

                MPlug plugX = depFn.findPlug("rotateX", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugX, times, rxValues, MFnAnimCurve::kAnimCurveTA, timeUnit);
                MPlug plugY = depFn.findPlug("rotateY", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugY, times, ryValues, MFnAnimCurve::kAnimCurveTA, timeUnit);
                MPlug plugZ = depFn.findPlug("rotateZ", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugZ, times, rzValues, MFnAnimCurve::kAnimCurveTA, timeUnit);
                break;
            }

            case cgltf_animation_path_type_translation:
            {
                std::vector<double> txValues, tyValues, tzValues;
                txValues.reserve(times.size());
                tyValues.reserve(times.size());
                tzValues.reserve(times.size());

                for (size_t k = 0; k < times.size(); ++k)
                {
                    const cgltf_size base = k * floatsPerKey + valueOffset;
                    MVector t(
                        static_cast<double>(floatValues[base + 0]),
                        static_cast<double>(floatValues[base + 1]),
                        static_cast<double>(floatValues[base + 2]));

                    if (applyTopLevelCorrection)
                        t = dcc_import_transform::ApplyToPoint(correction, t);

                    txValues.push_back(t.x);
                    tyValues.push_back(t.y);
                    tzValues.push_back(t.z);
                }

                MPlug plugX = depFn.findPlug("translateX", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugX, times, txValues, MFnAnimCurve::kAnimCurveTL, timeUnit);
                MPlug plugY = depFn.findPlug("translateY", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugY, times, tyValues, MFnAnimCurve::kAnimCurveTL, timeUnit);
                MPlug plugZ = depFn.findPlug("translateZ", true, &status);
                if (status) dcc_animation::SetCurveKeys(plugZ, times, tzValues, MFnAnimCurve::kAnimCurveTL, timeUnit);
                break;
            }

            case cgltf_animation_path_type_scale:
            {
                const int compCount = (numComponents > 3) ? 3 : static_cast<int>(numComponents);
                for (int comp = 0; comp < compCount; ++comp)
                {
                    const char *attrName = (comp == 0) ? "scaleX" : (comp == 1) ? "scaleY" : "scaleZ";
                    MPlug attrPlug = depFn.findPlug(attrName, true, &status);
                    if (!status) continue;

                    std::vector<double> values;
                    values.reserve(times.size());
                    for (size_t k = 0; k < times.size(); ++k)
                    {
                        double val = static_cast<double>(
                            floatValues[k * floatsPerKey + valueOffset + comp]);
                        if (applyTopLevelCorrection)
                            val *= correction.scale[comp];
                        values.push_back(val);
                    }

                    dcc_animation::SetCurveKeys(attrPlug, times, values, MFnAnimCurve::kAnimCurveTL, timeUnit);
                }
                break;
            }

            default:
                continue;
            }
        }
    }

    return MS::kSuccess;
}
