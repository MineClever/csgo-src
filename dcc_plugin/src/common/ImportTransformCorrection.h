#pragma once

#include <maya/MEulerRotation.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVector.h>

#include <string>
#include <unordered_map>

namespace dcc_import_transform
{

struct TransformCorrection
{
    MVector translation = MVector::zero;
    MEulerRotation rotation;
    double scale[3] = {1.0, 1.0, 1.0};

    bool IsIdentity() const;
    bool HasNonUniformScale() const;
    MQuaternion RotationQuaternion() const;
    MMatrix Matrix() const;
};

TransformCorrection ParseTransformCorrection(const std::unordered_map<std::string, std::string> &optionMap);
bool ParseDoubleOption(
    const std::unordered_map<std::string, std::string> &optionMap,
    const char *key,
    double &value);

MVector ApplyToPoint(const TransformCorrection &correction, const MVector &point);
MQuaternion ApplyToQuaternion(const TransformCorrection &correction, const MQuaternion &quaternion);
MStatus ApplyPreTransformToObject(const MObject &object, const MMatrix &preTransform);

} // namespace dcc_import_transform
