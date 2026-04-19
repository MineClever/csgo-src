#include "TransformCorrection.h"

#include <cmath>

#include <maya/MFnTransform.h>
#include <maya/MPoint.h>

namespace
{
constexpr double kEpsilon = 1.0e-8;
constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;
}

namespace dcc_transform
{

bool TransformCorrection::IsIdentity() const
{
    return std::fabs(translation.x) <= kEpsilon &&
        std::fabs(translation.y) <= kEpsilon &&
        std::fabs(translation.z) <= kEpsilon &&
        std::fabs(rotation.x) <= kEpsilon &&
        std::fabs(rotation.y) <= kEpsilon &&
        std::fabs(rotation.z) <= kEpsilon &&
        std::fabs(scale[0] - 1.0) <= kEpsilon &&
        std::fabs(scale[1] - 1.0) <= kEpsilon &&
        std::fabs(scale[2] - 1.0) <= kEpsilon;
}

bool TransformCorrection::HasNonUniformScale() const
{
    return std::fabs(scale[0] - scale[1]) > kEpsilon || std::fabs(scale[1] - scale[2]) > kEpsilon;
}

MQuaternion TransformCorrection::RotationQuaternion() const
{
    return rotation.asQuaternion();
}

MMatrix TransformCorrection::Matrix() const
{
    MTransformationMatrix transformMatrix;
    transformMatrix.setTranslation(translation, MSpace::kTransform);
    transformMatrix.setRotationQuaternion(
        RotationQuaternion().x,
        RotationQuaternion().y,
        RotationQuaternion().z,
        RotationQuaternion().w);
    transformMatrix.setScale(const_cast<double *>(scale), MSpace::kTransform);
    return transformMatrix.asMatrix();
}

bool ParseDoubleOption(
    const std::unordered_map<std::string, std::string> &optionMap,
    const char *key,
    double &value)
{
    const auto it = optionMap.find(key);
    if (it == optionMap.end())
    {
        return false;
    }

    try
    {
        value = std::stod(it->second);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

TransformCorrection ParseTransformCorrection(const std::unordered_map<std::string, std::string> &optionMap)
{
    TransformCorrection correction;

    ParseDoubleOption(optionMap, "translatex", correction.translation.x);
    ParseDoubleOption(optionMap, "translatey", correction.translation.y);
    ParseDoubleOption(optionMap, "translatez", correction.translation.z);

    double degreesX = 0.0;
    double degreesY = 0.0;
    double degreesZ = 0.0;
    ParseDoubleOption(optionMap, "rotatex", degreesX);
    ParseDoubleOption(optionMap, "rotatey", degreesY);
    ParseDoubleOption(optionMap, "rotatez", degreesZ);
    correction.rotation = MEulerRotation(
        degreesX * kRadiansPerDegree,
        degreesY * kRadiansPerDegree,
        degreesZ * kRadiansPerDegree,
        MEulerRotation::kXYZ);

    ParseDoubleOption(optionMap, "scalex", correction.scale[0]);
    ParseDoubleOption(optionMap, "scaley", correction.scale[1]);
    ParseDoubleOption(optionMap, "scalez", correction.scale[2]);

    return correction;
}

MVector ApplyToPoint(const TransformCorrection &correction, const MVector &point)
{
    const MPoint corrected = MPoint(point) * correction.Matrix();
    return MVector(corrected.x, corrected.y, corrected.z);
}

MVector ApplyToTranslationScale(const TransformCorrection &correction, const MVector &translation)
{
    return MVector(
        translation.x * correction.scale[0],
        translation.y * correction.scale[1],
        translation.z * correction.scale[2]);
}

MVector ApplyToTopLevelTranslation(const TransformCorrection &correction, const MVector &translation)
{
    MVector corrected = ApplyToTranslationScale(correction, translation);
    corrected = corrected.rotateBy(correction.RotationQuaternion());
    corrected += correction.translation;
    return corrected;
}

MVector ApplyToNormal(const TransformCorrection &correction, const MVector &normal)
{
    MVector corrected(
        std::fabs(correction.scale[0]) > kEpsilon ? normal.x / correction.scale[0] : normal.x,
        std::fabs(correction.scale[1]) > kEpsilon ? normal.y / correction.scale[1] : normal.y,
        std::fabs(correction.scale[2]) > kEpsilon ? normal.z / correction.scale[2] : normal.z);
    corrected = corrected.rotateBy(correction.RotationQuaternion());
    return corrected.length() > kEpsilon ? corrected.normal() : normal;
}

MQuaternion ApplyToQuaternion(const TransformCorrection &correction, const MQuaternion &quaternion)
{
    return quaternion * correction.RotationQuaternion();
}

MStatus ApplyPreTransformToObject(const MObject &object, const MMatrix &preTransform)
{
    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MTransformationMatrix currentMatrix(transformFn.transformation(&status).asMatrix());
    if (!status)
    {
        return MS::kFailure;
    }

    const MTransformationMatrix corrected(preTransform * currentMatrix.asMatrix());
    const MVector translation = corrected.getTranslation(MSpace::kTransform, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    const MQuaternion rotation = corrected.rotation();
    double correctedScale[3] = {1.0, 1.0, 1.0};
    corrected.getScale(correctedScale, MSpace::kTransform);

    status = transformFn.setTranslation(translation, MSpace::kTransform);
    if (!status)
    {
        return MS::kFailure;
    }

    status = transformFn.setRotation(rotation);
    if (!status)
    {
        return MS::kFailure;
    }

    status = transformFn.setScale(correctedScale);
    return status ? MS::kSuccess : MS::kFailure;
}

} // namespace dcc_transform

namespace dcc_export_transform
{

bool ExportTransformPolicy::IsIdentity() const
{
    return correction.IsIdentity();
}

std::string NormalizeUpAxisName(std::string axisName)
{
    for (char &ch : axisName)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return axisName == "Z" ? "Z" : "Y";
}

ExportTransformPolicy BuildExportTransformPolicy(const dcc_transform::TransformCorrection &correction)
{
    ExportTransformPolicy policy;
    policy.correction = correction;
    return policy;
}

MVector ApplyToPoint(const ExportTransformPolicy &policy, const MVector &point)
{
    return dcc_transform::ApplyToPoint(policy.correction, point);
}

MVector ApplyToDirection(const ExportTransformPolicy &policy, const MVector &direction)
{
    return dcc_transform::ApplyToNormal(policy.correction, direction);
}

MQuaternion ApplyToQuaternion(const ExportTransformPolicy &policy, const MQuaternion &quaternion)
{
    return dcc_transform::ApplyToQuaternion(policy.correction, quaternion);
}

MEulerRotation ApplyToEulerRotation(const ExportTransformPolicy &policy, const MEulerRotation &rotation)
{
    return ApplyToQuaternion(policy, rotation.asQuaternion()).asEulerRotation();
}

} // namespace dcc_export_transform
