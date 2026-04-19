#include "MaterialExportUtils.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFn.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dcc_material_export
{
MObject FindConnectedSourceNode(const MPlug &destinationPlug)
{
    MPlugArray connectedPlugs;
    if (destinationPlug.connectedTo(connectedPlugs, true, false) != MS::kSuccess || connectedPlugs.length() == 0)
    {
        return MObject::kNullObj;
    }

    for (unsigned int i = 0; i < connectedPlugs.length(); ++i)
    {
        const MObject node = connectedPlugs[i].node();
        if (!node.isNull())
        {
            return node;
        }
    }

    return MObject::kNullObj;
}

MObject FindSurfaceShaderNode(const MObject &shadingGroupObject)
{
    MStatus status;
    MFnDependencyNode shadingGroupFn(shadingGroupObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    MPlug surfaceShaderPlug = shadingGroupFn.findPlug("surfaceShader", true, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    return FindConnectedSourceNode(surfaceShaderPlug);
}

bool ReadStringAttribute(const MObject &nodeObject, const char *attributeName, std::string &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status)
    {
        return false;
    }

    MString mayaValue;
    status = attributePlug.getValue(mayaValue);
    if (!status || mayaValue.length() == 0)
    {
        return false;
    }

    value = mayaValue.asChar();
    return true;
}

std::vector<int> ExtractPolygonIndices(const MObject &componentObject)
{
    if (componentObject.isNull() || !componentObject.hasFn(MFn::kMeshPolygonComponent))
    {
        return {};
    }

    MStatus status;
    MFnSingleIndexedComponent componentFn(componentObject, &status);
    if (!status)
    {
        return {};
    }

    MIntArray elementIndices;
    status = componentFn.getElements(elementIndices);
    if (!status)
    {
        return {};
    }

    std::vector<int> polygonIndices;
    polygonIndices.reserve(elementIndices.length());
    for (unsigned int i = 0; i < elementIndices.length(); ++i)
    {
        polygonIndices.push_back(elementIndices[i]);
    }

    return polygonIndices;
}

bool GetMeshShadingAssignments(const MFnMesh &meshFn, unsigned int instanceNumber, MeshShadingAssignments &assignments)
{
    assignments.shadingGroups.clear();
    assignments.faceShaderIndices.clear();
    return meshFn.getConnectedShaders(instanceNumber, assignments.shadingGroups, assignments.faceShaderIndices) == MS::kSuccess;
}

bool TryGetAssignedShadingGroup(const MeshShadingAssignments &assignments, int polygonIndex, MObject &shadingGroupObject)
{
    shadingGroupObject = MObject::kNullObj;
    if (polygonIndex < 0 || polygonIndex >= static_cast<int>(assignments.faceShaderIndices.length()))
    {
        return false;
    }

    const int shaderIndex = assignments.faceShaderIndices[polygonIndex];
    if (shaderIndex < 0 || shaderIndex >= static_cast<int>(assignments.shadingGroups.length()))
    {
        return false;
    }

    shadingGroupObject = assignments.shadingGroups[shaderIndex];
    return !shadingGroupObject.isNull();
}

bool GetMeshSetAssignments(const MFnMesh &meshFn, unsigned int instanceNumber, MObjectArray &connectedSets, MObjectArray &setComponents)
{
    connectedSets.clear();
    setComponents.clear();
    return meshFn.getConnectedSetsAndMembers(instanceNumber, connectedSets, setComponents, true) == MS::kSuccess;
}

ShadingGroupMaterialInfo DescribeShadingGroupMaterial(const MObject &shadingGroupObject, const std::string &fallbackShadingGroupName)
{
    ShadingGroupMaterialInfo materialInfo;
    materialInfo.shadingGroupName = fallbackShadingGroupName;

    const MObject shaderObject = FindSurfaceShaderNode(shadingGroupObject);
    if (shaderObject.isNull())
    {
        return materialInfo;
    }

    MStatus status;
    MFnDependencyNode shaderNodeFn(shaderObject, &status);
    if (!status)
    {
        return materialInfo;
    }

    materialInfo.shaderName = shaderNodeFn.name().asChar();
    materialInfo.shaderType = shaderNodeFn.typeName().asChar();
    return materialInfo;
}

std::string ResolvePreferredMaterialName(
    const std::string &requestedMaterialName,
    const std::string &defaultMaterialName,
    const ShadingGroupMaterialInfo &materialInfo)
{
    if (!requestedMaterialName.empty() && requestedMaterialName != defaultMaterialName)
    {
        return requestedMaterialName;
    }

    if (!materialInfo.shaderName.empty())
    {
        return materialInfo.shaderName;
    }

    if (!materialInfo.shadingGroupName.empty())
    {
        return materialInfo.shadingGroupName;
    }

    return defaultMaterialName;
}

} // namespace dcc_material_export
