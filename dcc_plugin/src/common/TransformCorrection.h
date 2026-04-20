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

namespace dcc_transform
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

bool ParseDoubleOption(
    const std::unordered_map<std::string, std::string> &optionMap,
    const char *key,
    double &value);
TransformCorrection ParseTransformCorrection(const std::unordered_map<std::string, std::string> &optionMap);

MVector ApplyToPoint(const TransformCorrection &correction, const MVector &point);
MVector ApplyToTranslationScale(const TransformCorrection &correction, const MVector &translation);
MVector ApplyToTopLevelTranslation(const TransformCorrection &correction, const MVector &translation);
MVector ApplyToNormal(const TransformCorrection &correction, const MVector &normal);
MQuaternion ApplyToQuaternion(const TransformCorrection &correction, const MQuaternion &quaternion);
MStatus ApplyPreTransformToObject(const MObject &object, const MMatrix &preTransform);

} // namespace dcc_transform

namespace dcc_export_transform
{

struct ExportTransformPolicy
{
    dcc_transform::TransformCorrection correction;

    bool IsIdentity() const;
};

std::string NormalizeUpAxisName(std::string axisName);
ExportTransformPolicy BuildExportTransformPolicy(const dcc_transform::TransformCorrection &correction);

MVector ApplyToPoint(const ExportTransformPolicy &policy, const MVector &point);
MVector ApplyToDirection(const ExportTransformPolicy &policy, const MVector &direction);
MVector ApplyToTopLevelTranslation(const ExportTransformPolicy &policy, const MVector &translation);
MQuaternion ApplyToQuaternion(const ExportTransformPolicy &policy, const MQuaternion &quaternion);
MQuaternion ApplyToTopLevelQuaternion(const ExportTransformPolicy &policy, const MQuaternion &quaternion);
MEulerRotation ApplyToEulerRotation(const ExportTransformPolicy &policy, const MEulerRotation &rotation);
MEulerRotation ApplyToTopLevelEulerRotation(const ExportTransformPolicy &policy, const MEulerRotation &rotation);

} // namespace dcc_export_transform

namespace dcc_import_transform = dcc_transform;
