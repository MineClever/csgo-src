#pragma once

#include "TransformCorrection.h"

#include <maya/MEulerRotation.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

namespace dcc_export_document
{

dcc_transform::TransformCorrection BuildScaleOnlyCorrection(const dcc_transform::TransformCorrection &correction);

MVector ApplyLocalTranslation(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &translation);
MVector ApplyLocalNormal(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &normal);
MVector ApplyLocalTangent(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &tangent);
double ApplyLocalTangentHandedness(const dcc_export_transform::ExportTransformPolicy &policy, double tangentHandedness);
MVector ApplyBakedMeshPoint(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &point);
MVector ApplyBakedMeshNormal(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &normal);

MVector ApplyTopLevelTranslation(const dcc_export_transform::ExportTransformPolicy &policy, const MVector &translation);
MQuaternion ApplyTopLevelQuaternion(const dcc_export_transform::ExportTransformPolicy &policy, const MQuaternion &quaternion);
MEulerRotation ApplyTopLevelEulerRotation(const dcc_export_transform::ExportTransformPolicy &policy, const MEulerRotation &rotation);

} // namespace dcc_export_document
