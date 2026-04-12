#include "ImportTransformCorrection.h"

#include <cmath>

#include <maya/MFnTransform.h>
#include <maya/MPoint.h>

namespace
{
constexpr double kEpsilon = 1.0e-8;
constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;
}

namespace dcc_import_transform
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

MQuaternion ApplyToQuaternion(const TransformCorrection &correction, const MQuaternion &quaternion)
{
    return correction.RotationQuaternion() * quaternion;
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

} // namespace dcc_import_transform
