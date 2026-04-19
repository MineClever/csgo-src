#pragma once

#include <string>

#include <maya/MDagPath.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dcc_material
{

struct MaterialNodeNames
{
    MString shaderName;
    MString shadingGroupName;
};

MaterialNodeNames BuildMaterialNodeNames(
    const std::string &baseName,
    const char *fallbackBaseName);

MObject EnsureDependencyNodeOfType(
    const MString &nodeType,
    const MString &nodeName,
    MStatus &status);

MStatus EnsureSurfaceShaderBinding(
    const MString &shaderType,
    const MString &shaderName,
    const MString &shadingGroupName,
    MObject &shaderObject,
    MObject &shadingGroupObject);

MStatus AssignWholeMeshToShadingGroup(
    const MObject &meshObject,
    const MObject &shadingGroupObject);

MStatus AssignFacesToShadingGroup(
    const MDagPath &meshPath,
    const MIntArray &faceIds,
    const MObject &shadingGroupObject);

} // namespace dcc_material
