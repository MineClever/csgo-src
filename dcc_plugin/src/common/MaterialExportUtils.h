#pragma once

#include <string>
#include <vector>

#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>

namespace dcc_material_export
{

struct ShadingGroupMaterialInfo
{
    std::string shadingGroupName;
    std::string shaderName;
    std::string shaderType;
};

struct MeshShadingAssignments
{
    MObjectArray shadingGroups;
    MIntArray faceShaderIndices;
};

MObject FindConnectedSourceNode(const MPlug &destinationPlug);
MObject FindSurfaceShaderNode(const MObject &shadingGroupObject);
bool ReadStringAttribute(const MObject &nodeObject, const char *attributeName, std::string &value);
std::vector<int> ExtractPolygonIndices(const MObject &componentObject);
bool GetMeshShadingAssignments(const MFnMesh &meshFn, unsigned int instanceNumber, MeshShadingAssignments &assignments);
bool TryGetAssignedShadingGroup(const MeshShadingAssignments &assignments, int polygonIndex, MObject &shadingGroupObject);
bool GetMeshSetAssignments(const MFnMesh &meshFn, unsigned int instanceNumber, MObjectArray &connectedSets, MObjectArray &setComponents);
ShadingGroupMaterialInfo DescribeShadingGroupMaterial(const MObject &shadingGroupObject, const std::string &fallbackShadingGroupName);
std::string ResolvePreferredMaterialName(
    const std::string &requestedMaterialName,
    const std::string &defaultMaterialName,
    const ShadingGroupMaterialInfo &materialInfo);

} // namespace dcc_material_export
