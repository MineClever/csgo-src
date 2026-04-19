#include "DmxImportMeshMaterial.h"
#include "DmxImportInternals.h"

#include <common/MaterialUtils.h>
#include <common/MayaCommandUtils.h>

#include <algorithm>
#include <string>
#include <vector>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{


static int ParseUvChannelIndex(const std::string &attributeName)
{
    if (attributeName == "textureCoordinates")
    {
        return 0;
    }

    if (attributeName.rfind("texcoord$", 0) == 0)
    {
        const char *suffix = attributeName.c_str() + 9;
        if (*suffix == '\0')
        {
            return -1;
        }

        char *end = nullptr;
        const long parsedIndex = std::strtol(suffix, &end, 10);
        if (end && *end == '\0' && parsedIndex >= 1)
        {
            return static_cast<int>(parsedIndex);
        }
    }

    return -1;
}

std::vector<UvSetData> CollectUvSets(const simple_dmx::Element *vertexData)
{
    std::vector<UvSetData> uvSets;
    if (!vertexData)
    {
        return uvSets;
    }

    const std::vector<std::string> mayaUvSetNames = FindAttributeStringArray(vertexData, "mayaUvSetNames");
    for (const auto &entry : vertexData->attributes)
    {
        const int channelIndex = ParseUvChannelIndex(entry.first);
        if (channelIndex < 0 || entry.second.kind != simple_dmx::Attribute::Kind::StringArray)
        {
            continue;
        }

        UvSetData uvSet;
        uvSet.channelIndex = channelIndex;
        uvSet.attributeName = entry.first;
        uvSet.indexAttributeName = entry.first + "Indices";
        uvSet.values = entry.second.stringArray;
        uvSet.indices.reserve(FindAttributeStringArray(vertexData, uvSet.indexAttributeName.c_str()).size());
        for (const std::string &indexString : FindAttributeStringArray(vertexData, uvSet.indexAttributeName.c_str()))
        {
            const std::vector<double> values = ParseNumberList(indexString);
            if (!values.empty())
            {
                uvSet.indices.push_back(static_cast<int>(values[0]));
            }
        }

        if (channelIndex >= 0 && channelIndex < static_cast<int>(mayaUvSetNames.size()))
        {
            uvSet.mayaSetName = mayaUvSetNames[static_cast<size_t>(channelIndex)];
        }
        else
        {
            uvSet.mayaSetName = channelIndex == 0 ? "map1" : entry.first;
        }

        uvSets.push_back(std::move(uvSet));
    }

    std::sort(
        uvSets.begin(),
        uvSets.end(),
        [](const UvSetData &lhs, const UvSetData &rhs)
        {
            return lhs.channelIndex < rhs.channelIndex;
        });
    return uvSets;
}

const simple_dmx::Element *FindMeshVertexData(const simple_dmx::Document &document, const simple_dmx::Element *meshElement)
{
    if (const simple_dmx::Element *bindState = FindAttributeElement(document, meshElement, "bindState"))
    {
        return bindState;
    }

    for (const simple_dmx::Element *baseState : FindAttributeElementArray(document, meshElement, "baseStates"))
    {
        if (baseState && (baseState->name == "bind" || baseState->name == "Bind"))
        {
            return baseState;
        }
    }

    const std::vector<const simple_dmx::Element *> baseStates = FindAttributeElementArray(document, meshElement, "baseStates");
    return baseStates.empty() ? nullptr : baseStates.front();
}

MStatus AssignFaceSetMaterials(
    const MFnMesh &meshFn,
    const std::vector<FaceSetAssignment> &faceSetAssignments)
{
    if (faceSetAssignments.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MDagPath meshPath;
    status = meshFn.getPath(meshPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (const FaceSetAssignment &assignment : faceSetAssignments)
    {
        if (assignment.polygonCount <= 0 || assignment.shadingGroupName.empty())
        {
            continue;
        }

        std::string shadingGroupName = SanitizeNodeName(assignment.shadingGroupName);
        if (shadingGroupName.empty())
        {
            shadingGroupName = "dmxMaterialSet";
        }
        if (shadingGroupName.size() < 3 || shadingGroupName.substr(shadingGroupName.size() - 3) != "_SG")
        {
            shadingGroupName += "_SG";
        }
        const std::string materialName = assignment.materialName.empty() ? shadingGroupName : assignment.materialName;
        const std::string shaderType = assignment.shaderType.empty() ? "lambert" : assignment.shaderType;
        const dcc_material::MaterialNodeNames defaultNames =
            dcc_material::BuildMaterialNodeNames(materialName, "dmxMaterial");
        const MString shaderName = assignment.shaderName.empty() ?
            defaultNames.shaderName :
            SanitizeNodeName(assignment.shaderName).c_str();

        MObject shaderObject;
        MObject shadingGroupObject;
        status = dcc_material::EnsureSurfaceShaderBinding(
            shaderType.c_str(),
            shaderName,
            shadingGroupName.c_str(),
            shaderObject,
            shadingGroupObject);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MFnDependencyNode shaderNodeFn(shaderObject, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MPlug colorPlug = shaderNodeFn.findPlug("color", true, &status);
        if (status && !assignment.color.empty())
        {
            SetVector3Plug(colorPlug, assignment.color);
        }

        MPlug transparencyPlug = shaderNodeFn.findPlug("transparency", true, &status);
        if (status && !assignment.transparency.empty())
        {
            SetVector3Plug(transparencyPlug, assignment.transparency);
        }

        if (!assignment.diffuseTexture.empty() && colorPlug.isNull() == false)
        {
            status = dcc_material::AssignFileTextureToPlug(
                MString((std::string(shaderName.asChar()) + "_diffuseFile").c_str()),
                assignment.diffuseTexture.c_str(),
                colorPlug,
                false);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }

        MPlug normalCameraPlug = shaderNodeFn.findPlug("normalCamera", true, &status);
        const std::string normalOrBumpTexture = assignment.normalTexture.empty() ? assignment.bumpTexture : assignment.normalTexture;
        if (status && !normalOrBumpTexture.empty())
        {
            status = dcc_material::AssignNormalTextureToShader(
                shaderName,
                normalOrBumpTexture.c_str(),
                normalCameraPlug);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }

        MIntArray faceIds;
        for (int offset = 0; offset < assignment.polygonCount; ++offset)
        {
            faceIds.append(assignment.polygonStart + offset);
        }

        status = dcc_material::AssignFacesToShadingGroup(meshPath, faceIds, shadingGroupObject);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

} // namespace dmx_import_impl

