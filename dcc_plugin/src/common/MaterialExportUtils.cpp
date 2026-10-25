#include "MaterialExportUtils.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFn.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dcc_material_export
{
MObject FindConnectedSourceNode(const MPlug &destinationPlug)
{
    const MPlug sourcePlug = FindConnectedSourcePlug(destinationPlug);
    return sourcePlug.isNull() ? MObject::kNullObj : sourcePlug.node();
}

MPlug FindConnectedSourcePlug(const MPlug &destinationPlug)
{
    MPlugArray connectedPlugs;
    const auto tryReturnFirstConnectedPlug = [](const MPlug &plug, MPlug &outPlug) -> bool
    {
        MPlugArray directConnections;
        if (plug.connectedTo(directConnections, true, false) != MS::kSuccess)
        {
            return false;
        }

        for (unsigned int i = 0; i < directConnections.length(); ++i)
        {
            if (!directConnections[i].isNull())
            {
                outPlug = directConnections[i];
                return true;
            }
        }

        return false;
    };

    MPlug connectedPlug;
    if (tryReturnFirstConnectedPlug(destinationPlug, connectedPlug))
    {
        return connectedPlug;
    }

    if (destinationPlug.isCompound())
    {
        for (unsigned int childIndex = 0; childIndex < destinationPlug.numChildren(); ++childIndex)
        {
            const MPlug childSourcePlug = FindConnectedSourcePlug(destinationPlug.child(childIndex));
            if (!childSourcePlug.isNull())
            {
                return childSourcePlug;
            }
        }
    }

    if (destinationPlug.isChild())
    {
        const MPlug parentPlug = destinationPlug.parent();
        if (tryReturnFirstConnectedPlug(parentPlug, connectedPlug))
        {
            return connectedPlug;
        }
    }

    return MPlug();
}

MObject FindSurfaceShaderNode(const MObject &shadingGroupObject)
{
    MStatus status;
    MFnDependencyNode shadingGroupFn(shadingGroupObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (const char *plugName : {"surfaceShader", "aiSurfaceShader", "miMaterialShader"})
    {
        MPlug shaderPlug = shadingGroupFn.findPlug(plugName, true, &status);
        if (!status || shaderPlug.isNull())
        {
            continue;
        }

        const MPlug sourcePlug = FindConnectedSourcePlug(shaderPlug);
        const MObject shaderNode = sourcePlug.isNull() ? MObject::kNullObj : sourcePlug.node();
        if (!shaderNode.isNull())
        {
            return shaderNode;
        }

        if (shaderPlug.isDestination())
        {
            const MObject shaderPlugNode = shaderPlug.source().node();
            if (!shaderPlugNode.isNull())
            {
                return shaderPlugNode;
            }
        }
    }

    MObject mutableShadingGroupObject(shadingGroupObject);
    MItDependencyGraph iterator(
        mutableShadingGroupObject,
        MFn::kDependencyNode,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        const MObject nodeObject = iterator.thisNode(&status);
        if (!status || nodeObject.isNull() || nodeObject == shadingGroupObject)
        {
            continue;
        }

        return nodeObject;
    }

    return MObject::kNullObj;
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

bool ReadFloatAttribute(const MObject &nodeObject, const char *attributeName, float &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status || attributePlug.isNull())
    {
        return false;
    }

    value = attributePlug.asFloat(&status);
    return status;
}

bool ReadBoolAttribute(const MObject &nodeObject, const char *attributeName, bool &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status || attributePlug.isNull())
    {
        return false;
    }

    value = attributePlug.asBool(&status);
    return status;
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
