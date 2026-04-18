#include "SmdImportUtils.h"

#include <common_smd/MayaSmdCommon.h>

#include <cctype>
#include <string>

#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

namespace smd_import_impl
{

CurrentTimeGuard::CurrentTimeGuard()
    : previousTime_(MAnimControl::currentTime())
{
}

CurrentTimeGuard::~CurrentTimeGuard()
{
    MAnimControl::setCurrentTime(previousTime_);
}

bool IsEmptyLayerName(const std::string &layerName)
{
    return layerName.empty() || layerName == "None" || layerName == "none";
}

std::string SanitizeNodeName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("smd_node") : value;
}

MStatus SetPoseOnObject(
    MObject object,
    const simple_smd::SkeletonPose &pose,
    const dcc_import_transform::TransformCorrection *correction)
{
    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access transform for skeleton pose.", status);
    }

    status = transformFn.setTranslation(MVector(pose.tx, pose.ty, pose.tz), MSpace::kTransform);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton translation.", status);
    }

    status = transformFn.setRotation(MEulerRotation(pose.rx, pose.ry, pose.rz));
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton rotation.", status);
    }

    if (correction && !correction->IsIdentity())
    {
        status = dcc_import_transform::ApplyPreTransformToObject(object, correction->Matrix());
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to apply top-level import correction to skeleton pose.", status);
        }
    }

    return MS::kSuccess;
}

} // namespace smd_import_impl
