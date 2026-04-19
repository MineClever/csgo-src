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

} // namespace dcc_material
