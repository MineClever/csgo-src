#pragma once

#include <common/SimpleDmxDocument.h>

#include <string>
#include <vector>

#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{

struct FaceSetAssignment
{
    std::string shadingGroupName;
    std::string materialName;
    std::string shaderName;
    std::string shaderType;
    std::string color;
    std::string transparency;
    std::string diffuseTexture;
    std::string normalTexture;
    std::string bumpTexture;
    int polygonStart = 0;
    int polygonCount = 0;
};

struct UvSetData
{
    int channelIndex = 0;
    std::string attributeName;
    std::string indexAttributeName;
    std::string mayaSetName;
    std::vector<std::string> values;
    std::vector<int> indices;
    MIntArray polygonVertexIndices;
};

const simple_dmx::Element *FindMeshVertexData(const simple_dmx::Document &document, const simple_dmx::Element *meshElement);
std::vector<UvSetData> CollectUvSets(const simple_dmx::Element *vertexData);
MStatus AssignFaceSetMaterials(
    const MFnMesh &meshFn,
    const std::vector<FaceSetAssignment> &faceSetAssignments);

} // namespace dmx_import_impl
