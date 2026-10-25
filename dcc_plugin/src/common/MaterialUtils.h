#pragma once

#include <string>

#include <maya/MDagPath.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dcc_material
{

enum class TextureChannel
{
    Color,
    Alpha,
    Red,
    Green,
    Blue
};

enum class TextureColorSpace
{
    Color,
    Raw
};

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

MStatus AssignFileTextureToPlug(
    const MString &fileNodeName,
    const MString &texturePath,
    const MPlug &destinationPlug,
    bool useAlphaOutput,
    TextureColorSpace colorSpace = TextureColorSpace::Color);

MStatus AssignTextureChannelToPlug(
    const MString &fileNodeName,
    const MString &texturePath,
    const MPlug &destinationPlug,
    TextureChannel channel,
    TextureColorSpace colorSpace = TextureColorSpace::Color);

MStatus AssignNormalTextureToShader(
    const MString &shaderBaseName,
    const MString &texturePath,
    const MPlug &normalCameraPlug);

MStatus EnsureStringAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    const MString &value);

MStatus EnsureFloatAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    float value);

MStatus EnsureBoolAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    bool value);

} // namespace dcc_material
