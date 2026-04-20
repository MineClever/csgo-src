#include "ExportTransformUtils.h"

#include <cmath>

namespace dcc_export_document
{
namespace
{
constexpr double kCorrectionEpsilon = 1.0e-8;
}

dcc_transform::TransformCorrection BuildScaleOnlyCorrection(const dcc_transform::TransformCorrection &correction)
{
    dcc_transform::TransformCorrection scaleOnlyCorrection;
    scaleOnlyCorrection.scale[0] = correction.scale[0];
    scaleOnlyCorrection.scale[1] = correction.scale[1];
    scaleOnlyCorrection.scale[2] = correction.scale[2];
    return scaleOnlyCorrection;
}

MVector ApplyLocalTranslation(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &translation)
{
    return dcc_transform::ApplyToTranslationScale(policy.correction, translation);
}

MVector ApplyLocalNormal(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &normal)
{
    return dcc_transform::ApplyToNormal(BuildScaleOnlyCorrection(policy.correction), normal);
}

MVector ApplyLocalTangent(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &tangent)
{
    MVector corrected(
        tangent.x * policy.correction.scale[0],
        tangent.y * policy.correction.scale[1],
        tangent.z * policy.correction.scale[2]);
    return corrected.length() > kCorrectionEpsilon ? corrected.normal() : tangent;
}

double ApplyLocalTangentHandedness(const dcc_export_transform::ExportTransformPolicy &policy, double tangentHandedness)
{
    const double determinantSign =
        policy.correction.scale[0] * policy.correction.scale[1] * policy.correction.scale[2];
    return determinantSign < 0.0 ? -tangentHandedness : tangentHandedness;
}

MVector ApplyBakedMeshPoint(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &point)
{
    return dcc_export_transform::ApplyToPoint(policy, point);
}

MVector ApplyBakedMeshNormal(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &normal)
{
    return dcc_export_transform::ApplyToDirection(policy, normal);
}

MVector ApplyTopLevelTranslation(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &translation)
{
    return dcc_export_transform::ApplyToTopLevelTranslation(policy, translation);
}

MQuaternion ApplyTopLevelQuaternion(const dcc_export_transform::ExportTransformPolicy &policy, const MQuaternion &quaternion)
{
    return dcc_export_transform::ApplyToTopLevelQuaternion(policy, quaternion);
}

MEulerRotation ApplyTopLevelEulerRotation(const dcc_export_transform::ExportTransformPolicy &policy, const MEulerRotation &rotation)
{
    return dcc_export_transform::ApplyToTopLevelEulerRotation(policy, rotation);
}

} // namespace dcc_export_document
