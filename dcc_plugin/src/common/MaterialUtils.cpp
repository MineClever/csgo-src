#include "MaterialUtils.h"

#include "MayaCommandUtils.h"

#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

namespace dcc_material
{

namespace
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
            return existingNode;
        }
    }

    MObject nodeObject;
    status = maya_cmd::CreateNamedDependencyNode(nodeType, nodeName, nodeObject);
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
    bool useAlphaOutput)
{
    if (texturePath.length() == 0 || destinationPlug.isNull())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MObject fileNodeObject = EnsureDependencyNodeOfType("file", fileNodeName, status);
    if (!status || fileNodeObject.isNull())
    {
        return MS::kFailure;
    }

    MFnDependencyNode fileNodeFn(fileNodeObject, &status);
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

    MPlug outputPlug = fileNodeFn.findPlug(useAlphaOutput ? "outAlpha" : "outColor", true, &status);
    if (!status)
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

    status = AssignFileTextureToPlug(shaderBaseName + "_normalFile", texturePath, bumpValuePlug, true);
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

} // namespace dcc_material
