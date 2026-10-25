#include "MaterialUtils.h"

#include "MayaCommandUtils.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>

namespace dcc_material
{

namespace detail
{

std::string SanitizeMaterialNodeName(std::string value, const char *fallback)
{
    if (value.empty())
    {
        value = fallback;
    }

    for (char &character : value)
    {
        const bool isAlphaNum =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_';
        if (!isAlphaNum)
        {
            character = '_';
        }
    }

    return value.empty() ? std::string(fallback) : value;
}

} // namespace detail

using namespace detail;

namespace
{

MStatus EnsurePlace2dConnection(MFnDependencyNode &fileNodeFn, const MString &baseNodeName)
{
    MStatus status;

    MPlug fileUvPlug = fileNodeFn.findPlug("uvCoord", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject place2dObject = EnsureDependencyNodeOfType("place2dTexture", baseNodeName + "_place2d", status);
    if (!status || place2dObject.isNull())
    {
        return MS::kFailure;
    }

    MFnDependencyNode place2dFn(place2dObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug outUvPlug = place2dFn.findPlug("outUV", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    status = maya_cmd::ConnectPlugsForce(outUvPlug, fileUvPlug);
    if (!status)
    {
        return status;
    }

    MPlug outUvFilterSizePlug = place2dFn.findPlug("outUvFilterSize", true, &status);
    if (!status)
    {
        return MS::kSuccess;
    }

    MPlug uvFilterSizePlug = fileNodeFn.findPlug("uvFilterSize", true, &status);
    if (!status)
    {
        return MS::kSuccess;
    }

    return maya_cmd::ConnectPlugsForce(outUvFilterSizePlug, uvFilterSizePlug);
}

MStatus IgnoreFileTextureColorSpaceRules(MFnDependencyNode &fileNodeFn)
{
    MStatus status;
    MPlug ignoreRulesPlug = fileNodeFn.findPlug("ignoreColorSpaceFileRules", true, &status);
    if (status && !ignoreRulesPlug.isNull())
    {
        status = ignoreRulesPlug.setBool(true);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SetFileTextureColorSpace(MFnDependencyNode &fileNodeFn, TextureColorSpace colorSpace)
{
    MStatus status;
    MPlug colorSpacePlug = fileNodeFn.findPlug("colorSpace", true, &status);
    if (!status || colorSpacePlug.isNull())
    {
        return MS::kSuccess;
    }

    const MString mayaColorSpace =
        (colorSpace == TextureColorSpace::Raw) ? MString("Raw") : MString("sRGB");
    status = colorSpacePlug.setString(mayaColorSpace);
    return status ? MS::kSuccess : MS::kFailure;
}

MStatus EnsureFileTextureNode(
    const MString &fileNodeName,
    const MString &texturePath,
    TextureColorSpace colorSpace,
    MFnDependencyNode &fileNodeFn)
{
    MStatus status;
    MObject fileNodeObject = EnsureDependencyNodeOfType("file", fileNodeName, status);
    if (!status || fileNodeObject.isNull())
    {
        return MS::kFailure;
    }

    fileNodeFn.setObject(fileNodeObject);

    // Ignore color space file rules before assigning the texture path so Maya
    // does not auto-derive (and later re-apply) a colorSpace that overrides
    // the explicit value set below when fileTextureName changes.
    status = IgnoreFileTextureColorSpaceRules(fileNodeFn);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug fileTextureNamePlug = fileNodeFn.findPlug("fileTextureName", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    status = fileTextureNamePlug.setString(texturePath);
    if (!status)
    {
        return MS::kFailure;
    }

    status = SetFileTextureColorSpace(fileNodeFn, colorSpace);
    if (!status)
    {
        return MS::kFailure;
    }

    status = maya_cmd::EnsureTextureRemovedFromDefaultTextureList(fileNodeObject);
    if (!status)
    {
        return MS::kFailure;
    }

    return EnsurePlace2dConnection(fileNodeFn, fileNodeName);
}

MPlug GetTextureOutputPlug(
    MFnDependencyNode &fileNodeFn,
    TextureChannel channel,
    MStatus &status)
{
    if (channel == TextureChannel::Color)
    {
        return fileNodeFn.findPlug("outColor", true, &status);
    }

    if (channel == TextureChannel::Alpha)
    {
        return fileNodeFn.findPlug("outAlpha", true, &status);
    }

    MPlug outColorPlug = fileNodeFn.findPlug("outColor", true, &status);
    if (!status || outColorPlug.numChildren() < 3)
    {
        return MPlug();
    }

    switch (channel)
    {
    case TextureChannel::Red:
        return outColorPlug.child(0);
    case TextureChannel::Green:
        return outColorPlug.child(1);
    case TextureChannel::Blue:
        return outColorPlug.child(2);
    default:
        return outColorPlug;
    }
}

template <typename AttributeFnT, typename CreateFnT, typename SetValueFnT>
MStatus EnsureDynamicAttributeValue(
    const MObject &nodeObject,
    const char *attributeName,
    CreateFnT createAttribute,
    SetValueFnT setValue)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject attributeObject = nodeFn.attribute(attributeName, &status);
    if (!status)
    {
        AttributeFnT attributeFn;
        attributeObject = createAttribute(attributeFn, status);
        if (!status)
        {
            return MS::kFailure;
        }

        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status || attributePlug.isNull())
    {
        return MS::kFailure;
    }

    return setValue(attributePlug);
}

} // namespace

MaterialNodeNames BuildMaterialNodeNames(
    const std::string &baseName,
    const char *fallbackBaseName)
{
    MaterialNodeNames names;
    const std::string sanitizedBaseName = SanitizeMaterialNodeName(baseName, fallbackBaseName);
    names.shaderName = sanitizedBaseName.c_str();
    names.shadingGroupName = (sanitizedBaseName + "_SG").c_str();
    return names;
}

MObject EnsureDependencyNodeOfType(
    const MString &nodeType,
    const MString &nodeName,
    MStatus &status)
{
    MObject existingNode;
    if (maya_cmd::TryGetNodeByName(nodeName, existingNode) && !existingNode.isNull())
    {
        MFnDependencyNode existingNodeFn(existingNode, &status);
        if (!status)
        {
            return MObject::kNullObj;
        }

        if (existingNodeFn.typeName() == nodeType)
        {
            maya_cmd::EnsureNodeRegisteredInDefaultRenderUtilityList(existingNode);
            return existingNode;
        }
    }

    MObject nodeObject;
    status = maya_cmd::CreateNamedDependencyNode(nodeType, nodeName, nodeObject);
    if (status && !nodeObject.isNull())
    {
        maya_cmd::EnsureNodeRegisteredInDefaultRenderUtilityList(nodeObject);
    }
    return status ? nodeObject : MObject::kNullObj;
}

MStatus EnsureSurfaceShaderBinding(
    const MString &shaderType,
    const MString &shaderName,
    const MString &shadingGroupName,
    MObject &shaderObject,
    MObject &shadingGroupObject)
{
    MStatus status;

    shaderObject = EnsureDependencyNodeOfType(shaderType, shaderName, status);
    if (!status || shaderObject.isNull())
    {
        return MS::kFailure;
    }

    status = maya_cmd::EnsureShaderRegisteredInDefaultShaderList(shaderObject);
    if (!status)
    {
        return MS::kFailure;
    }

    status = maya_cmd::EnsureRenderableShadingGroup(shadingGroupName, shadingGroupObject);
    if (!status || shadingGroupObject.isNull())
    {
        return MS::kFailure;
    }

    MFnDependencyNode shaderNodeFn(shaderObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnDependencyNode shadingGroupNodeFn(shadingGroupObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug outColorPlug = shaderNodeFn.findPlug("outColor", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug surfaceShaderPlug = shadingGroupNodeFn.findPlug("surfaceShader", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return maya_cmd::ConnectPlugsForce(outColorPlug, surfaceShaderPlug);
}

MStatus AssignWholeMeshToShadingGroup(
    const MObject &meshObject,
    const MObject &shadingGroupObject)
{
    if (meshObject.isNull() || shadingGroupObject.isNull())
    {
        return MS::kFailure;
    }

    MDagPath meshPath;
    MStatus status = MDagPath::getAPathTo(meshObject, meshPath);
    if (!status)
    {
        return MS::kFailure;
    }

    return maya_cmd::AddDagPathToSet(meshPath, shadingGroupObject);
}

MStatus AssignFacesToShadingGroup(
    const MDagPath &meshPath,
    const MIntArray &faceIds,
    const MObject &shadingGroupObject)
{
    if (!meshPath.isValid() || shadingGroupObject.isNull() || faceIds.length() == 0)
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnSingleIndexedComponent componentFn;
    MObject faceComponent = componentFn.create(MFn::kMeshPolygonComponent, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MIntArray mutableFaceIds(faceIds);
    status = componentFn.addElements(mutableFaceIds);
    if (!status)
    {
        return MS::kFailure;
    }

    return maya_cmd::AddComponentToSet(meshPath, faceComponent, shadingGroupObject);
}

MStatus AssignFileTextureToPlug(
    const MString &fileNodeName,
    const MString &texturePath,
    const MPlug &destinationPlug,
    bool useAlphaOutput,
    TextureColorSpace colorSpace)
{
    if (texturePath.length() == 0 || destinationPlug.isNull())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode fileNodeFn;
    status = EnsureFileTextureNode(fileNodeName, texturePath, colorSpace, fileNodeFn);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug outputPlug = fileNodeFn.findPlug(useAlphaOutput ? "outAlpha" : "outColor", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return maya_cmd::ConnectPlugsForce(outputPlug, destinationPlug);
}

MStatus AssignTextureChannelToPlug(
    const MString &fileNodeName,
    const MString &texturePath,
    const MPlug &destinationPlug,
    TextureChannel channel,
    TextureColorSpace colorSpace)
{
    if (texturePath.length() == 0 || destinationPlug.isNull())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode fileNodeFn;
    status = EnsureFileTextureNode(fileNodeName, texturePath, colorSpace, fileNodeFn);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug outputPlug = GetTextureOutputPlug(fileNodeFn, channel, status);
    if (!status || outputPlug.isNull())
    {
        return MS::kFailure;
    }

    return maya_cmd::ConnectPlugsForce(outputPlug, destinationPlug);
}

MStatus AssignNormalTextureToShader(
    const MString &shaderBaseName,
    const MString &texturePath,
    const MPlug &normalCameraPlug)
{
    if (texturePath.length() == 0 || normalCameraPlug.isNull())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MObject bumpNodeObject = EnsureDependencyNodeOfType("bump2d", shaderBaseName + "_normalBump", status);
    if (!status || bumpNodeObject.isNull())
    {
        return MS::kFailure;
    }

    MFnDependencyNode bumpNodeFn(bumpNodeObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug bumpInterpPlug = bumpNodeFn.findPlug("bumpInterp", true, &status);
    if (status)
    {
        bumpInterpPlug.setInt(1);
    }

    MPlug bumpValuePlug = bumpNodeFn.findPlug("bumpValue", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    status = AssignFileTextureToPlug(
        shaderBaseName + "_normalFile",
        texturePath,
        bumpValuePlug,
        true,
        TextureColorSpace::Raw);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug outNormalPlug = bumpNodeFn.findPlug("outNormal", true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return maya_cmd::ConnectPlugsForce(outNormalPlug, normalCameraPlug);
}

MStatus EnsureStringAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    const MString &value)
{
    return EnsureDynamicAttributeValue<MFnTypedAttribute>(
        nodeObject,
        attributeName,
        [attributeName](MFnTypedAttribute &attributeFn, MStatus &status) -> MObject
        {
            MFnStringData stringDataFn;
            MObject defaultValue = stringDataFn.create("", &status);
            if (!status)
            {
                return MObject::kNullObj;
            }

            MObject attributeObject = attributeFn.create(
                attributeName,
                attributeName,
                MFnData::kString,
                defaultValue,
                &status);
            if (status)
            {
                attributeFn.setReadable(true);
                attributeFn.setWritable(true);
                attributeFn.setStorable(true);
                attributeFn.setKeyable(false);
            }
            return attributeObject;
        },
        [&value](MPlug &attributePlug) -> MStatus
        {
            return attributePlug.setString(value);
        });
}

MStatus EnsureFloatAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    float value)
{
    return EnsureDynamicAttributeValue<MFnNumericAttribute>(
        nodeObject,
        attributeName,
        [attributeName](MFnNumericAttribute &attributeFn, MStatus &status) -> MObject
        {
            MObject attributeObject = attributeFn.create(
                attributeName,
                attributeName,
                MFnNumericData::kFloat,
                0.0f,
                &status);
            if (status)
            {
                attributeFn.setReadable(true);
                attributeFn.setWritable(true);
                attributeFn.setStorable(true);
                attributeFn.setKeyable(false);
            }
            return attributeObject;
        },
        [value](MPlug &attributePlug) -> MStatus
        {
            return attributePlug.setFloat(value);
        });
}

MStatus EnsureBoolAttribute(
    const MObject &nodeObject,
    const char *attributeName,
    bool value)
{
    return EnsureDynamicAttributeValue<MFnNumericAttribute>(
        nodeObject,
        attributeName,
        [attributeName](MFnNumericAttribute &attributeFn, MStatus &status) -> MObject
        {
            MObject attributeObject = attributeFn.create(
                attributeName,
                attributeName,
                MFnNumericData::kBoolean,
                false,
                &status);
            if (status)
            {
                attributeFn.setReadable(true);
                attributeFn.setWritable(true);
                attributeFn.setStorable(true);
                attributeFn.setKeyable(false);
            }
            return attributeObject;
        },
        [value](MPlug &attributePlug) -> MStatus
        {
            return attributePlug.setBool(value);
        });
}

} // namespace dcc_material
