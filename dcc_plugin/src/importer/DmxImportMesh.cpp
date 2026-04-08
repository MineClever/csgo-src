#include "DmxImportMesh.h"

#include "DmxImportDeformers.h"
#include "DmxImportMeshMaterial.h"

#include <sstream>
#include <string>
#include <vector>

#include <maya/MFloatArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MVectorArray.h>

namespace dmx_import_impl
{

static MStatus SetOrCreateStringAttribute(const MObject &nodeObject, const char *attributeName, const std::string &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return status;
    }

    MObject attributeObject = nodeFn.attribute(attributeName, &status);
    if (!status || attributeObject.isNull())
    {
        MFnTypedAttribute typedAttributeFn;
        attributeObject = typedAttributeFn.create(attributeName, attributeName, MFnData::kString, MObject::kNullObj, &status);
        if (!status)
        {
            return status;
        }
        typedAttributeFn.setHidden(true);
        typedAttributeFn.setStorable(true);
        typedAttributeFn.setReadable(true);
        typedAttributeFn.setWritable(true);
        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return status;
        }
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status)
    {
        return status;
    }

    return attributePlug.setString(value.c_str());
}

static std::string JoinLines(const std::vector<std::string> &values)
{
    std::ostringstream stream;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            stream << '\n';
        }
        stream << values[i];
    }
    return stream.str();
}

MStatus CreateMeshShape(ImportContext &context, const simple_dmx::Element *dagElement, MObject parent)
{
    const simple_dmx::Document &document = context.document;
    const simple_dmx::Element *meshElement = FindAttributeElement(document, dagElement, "shape");
    if (!meshElement || meshElement->type != "DmeMesh")
    {
        return MS::kSuccess;
    }

    const simple_dmx::Element *vertexData = FindMeshVertexData(document, meshElement);
    if (!vertexData)
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh has no bind/base state: ") + dagElement->name.c_str());
    }

    const std::vector<std::string> positionStrings = FindAttributeStringArray(vertexData, "positions");
    const std::vector<std::string> positionIndexStrings = FindAttributeStringArray(vertexData, "positionsIndices");
    if (positionStrings.empty() || positionIndexStrings.empty())
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh is missing positions or positionsIndices: ") + dagElement->name.c_str());
    }

    MPointArray points;
    for (const std::string &positionString : positionStrings)
    {
        const std::vector<double> values = ParseNumberList(positionString);
        if (values.size() < 3)
        {
            continue;
        }

        points.append(values[0], values[1], values[2]);
    }

    std::vector<int> positionIndices;
    positionIndices.reserve(positionIndexStrings.size());
    for (const std::string &indexString : positionIndexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (values.empty())
        {
            continue;
        }

        positionIndices.push_back(static_cast<int>(values[0]));
    }

    const std::vector<std::string> normalStrings = FindAttributeStringArray(vertexData, "normals");
    const std::vector<std::string> normalIndexStrings = FindAttributeStringArray(vertexData, "normalsIndices");
    std::vector<int> normalIndices;
    normalIndices.reserve(normalIndexStrings.size());
    for (const std::string &indexString : normalIndexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (!values.empty())
        {
            normalIndices.push_back(static_cast<int>(values[0]));
        }
    }

    std::vector<UvSetData> uvSets = CollectUvSets(vertexData);
    const std::vector<std::string> tangentStrings = FindAttributeStringArray(vertexData, "tangents");
    const std::vector<std::string> tangentIndexStrings = FindAttributeStringArray(vertexData, "tangentsIndices");

    const bool flipV = FindAttributeString(vertexData, "flipVCoordinates") == "1" ||
        FindAttributeString(vertexData, "flipVCoordinates") == "true";

    MIntArray polygonCounts;
    MIntArray polygonConnects;
    MIntArray faceIds;
    MIntArray normalVertexIds;
    MVectorArray faceVertexNormals;
    std::vector<FaceSetAssignment> faceSetAssignments;
    for (const simple_dmx::Element *faceSet : FindAttributeElementArray(document, meshElement, "faceSets"))
    {
        const int polygonStart = polygonCounts.length();
        const std::vector<std::string> faceStrings = FindAttributeStringArray(faceSet, "faces");
        std::vector<int> faceIndices;
        faceIndices.reserve(faceStrings.size());
        for (const std::string &faceString : faceStrings)
        {
            const std::vector<double> values = ParseNumberList(faceString);
            if (values.empty())
            {
                continue;
            }

            faceIndices.push_back(static_cast<int>(values[0]));
        }

        int polygonVertexCount = 0;
        int polygonIndex = polygonCounts.length();
        unsigned int polygonConnectStart = polygonConnects.length();
        for (int faceVertexIndex : faceIndices)
        {
            if (faceVertexIndex == -1)
            {
                if (polygonVertexCount >= 3)
                {
                    polygonCounts.append(polygonVertexCount);
                    ++polygonIndex;
                }
                else if (polygonVertexCount > 0)
                {
                    for (unsigned int i = 0; i < static_cast<unsigned int>(polygonVertexCount); ++i)
                    {
                        polygonConnects.remove(polygonConnects.length() - 1);
                        if (faceVertexNormals.length() > 0)
                        {
                            faceVertexNormals.remove(faceVertexNormals.length() - 1);
                            faceIds.remove(faceIds.length() - 1);
                            normalVertexIds.remove(normalVertexIds.length() - 1);
                        }
                        for (UvSetData &uvSet : uvSets)
                        {
                            if (uvSet.polygonVertexIndices.length() > 0)
                            {
                                uvSet.polygonVertexIndices.remove(uvSet.polygonVertexIndices.length() - 1);
                            }
                        }
                    }
                }

                polygonVertexCount = 0;
                polygonConnectStart = polygonConnects.length();
                continue;
            }

            if (faceVertexIndex < 0 || faceVertexIndex >= static_cast<int>(positionIndices.size()))
            {
                continue;
            }

            const int pointIndex = positionIndices[faceVertexIndex];
            if (pointIndex < 0 || pointIndex >= static_cast<int>(points.length()))
            {
                continue;
            }

            polygonConnects.append(pointIndex);
            if (!normalStrings.empty() && faceVertexIndex < static_cast<int>(normalIndices.size()))
            {
                const int normalIndex = normalIndices[faceVertexIndex];
                if (normalIndex >= 0 && normalIndex < static_cast<int>(normalStrings.size()))
                {
                    const std::vector<double> normalValues = ParseNumberList(normalStrings[normalIndex]);
                    if (normalValues.size() >= 3)
                    {
                        faceVertexNormals.append(MVector(
                            normalValues[0],
                            normalValues[1],
                            normalValues[2]));
                        faceIds.append(polygonIndex);
                        normalVertexIds.append(pointIndex);
                    }
                }
            }

            for (UvSetData &uvSet : uvSets)
            {
                if (faceVertexIndex < static_cast<int>(uvSet.indices.size()))
                {
                    const int uvIndex = uvSet.indices[faceVertexIndex];
                    if (uvIndex >= 0 && uvIndex < static_cast<int>(uvSet.values.size()))
                    {
                        uvSet.polygonVertexIndices.append(uvIndex);
                    }
                }
            }

            ++polygonVertexCount;
        }

        if (polygonVertexCount >= 3)
        {
            polygonCounts.append(polygonVertexCount);
        }
        else if (polygonVertexCount > 0)
        {
            while (polygonConnects.length() > polygonConnectStart)
            {
                polygonConnects.remove(polygonConnects.length() - 1);
            }
        }

        const int polygonEnd = polygonCounts.length();
        if (polygonEnd > polygonStart)
        {
            FaceSetAssignment assignment;
            assignment.shadingGroupName = faceSet ? faceSet->name : std::string();
            if (const simple_dmx::Element *materialElement = FindAttributeElement(document, faceSet, "material"))
            {
                assignment.materialName = materialElement->name;
                const std::string materialSlotName = FindAttributeString(materialElement, "mtlName");
                if (!materialSlotName.empty())
                {
                    assignment.materialName = materialSlotName;
                }
                assignment.shaderName = FindAttributeString(materialElement, "mayaShaderName");
                assignment.shaderType = FindAttributeString(materialElement, "mayaShaderType");
                assignment.color = FindAttributeString(materialElement, "mayaColor");
                assignment.transparency = FindAttributeString(materialElement, "mayaTransparency");
                assignment.diffuseTexture = FindAttributeString(materialElement, "mayaDiffuseTexture");
                assignment.normalTexture = FindAttributeString(materialElement, "mayaNormalTexture");
                assignment.bumpTexture = FindAttributeString(materialElement, "mayaBumpTexture");
            }
            assignment.polygonStart = polygonStart;
            assignment.polygonCount = polygonEnd - polygonStart;
            faceSetAssignments.push_back(std::move(assignment));
        }
    }

    if (points.length() == 0 || polygonCounts.length() == 0 || polygonConnects.length() == 0)
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh geometry was empty after parsing: ") + dagElement->name.c_str());
    }

    MStatus status;
    MFnMesh meshFn;
    const MObject meshObject = meshFn.create(points.length(), polygonCounts.length(), points, polygonCounts, polygonConnects, parent, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    meshFn.setName((dagElement->name.empty() ? std::string("dmx_meshShape") : dagElement->name + "Shape").c_str());

    for (size_t uvSetIndex = 0; uvSetIndex < uvSets.size(); ++uvSetIndex)
    {
        MFloatArray uValues;
        MFloatArray vValues;
        for (const std::string &uvString : uvSets[uvSetIndex].values)
        {
            const std::vector<double> values = ParseNumberList(uvString);
            if (values.size() < 2)
            {
                uValues.append(0.0f);
                vValues.append(0.0f);
                continue;
            }

            uValues.append(static_cast<float>(values[0]));
            float v = static_cast<float>(values[1]);
            if (flipV)
            {
                v = 1.0f - v;
            }
            vValues.append(v);
        }

        if (uvSets[uvSetIndex].polygonVertexIndices.length() != polygonConnects.length())
        {
            continue;
        }

        MString uvSetName = uvSets[uvSetIndex].mayaSetName.c_str();
        if (uvSetName.length() == 0)
        {
            uvSetName = uvSetIndex == 0 ? meshFn.currentUVSetName() : MString(uvSets[uvSetIndex].attributeName.c_str());
        }

        if (uvSetIndex == 0)
        {
            const MString currentUvSetName = meshFn.currentUVSetName();
            if (uvSetName != currentUvSetName)
            {
                meshFn.renameUVSet(currentUvSetName, uvSetName);
            }
        }
        else
        {
            MStringArray existingUvSetNames;
            meshFn.getUVSetNames(existingUvSetNames);
            bool uvSetExists = false;
            for (unsigned int existingIndex = 0; existingIndex < existingUvSetNames.length(); ++existingIndex)
            {
                if (existingUvSetNames[existingIndex] == uvSetName)
                {
                    uvSetExists = true;
                    break;
                }
            }
            if (!uvSetExists)
            {
                meshFn.createUVSetWithName(uvSetName);
            }
        }

        status = meshFn.setUVs(uValues, vValues, &uvSetName);
        if (status)
        {
            status = meshFn.assignUVs(polygonCounts, uvSets[uvSetIndex].polygonVertexIndices, &uvSetName);
        }
        if (!status)
        {
            return status;
        }
    }

    if (faceVertexNormals.length() == polygonConnects.length() && faceIds.length() == polygonConnects.length() && normalVertexIds.length() == polygonConnects.length())
    {
        status = meshFn.setFaceVertexNormals(faceVertexNormals, faceIds, normalVertexIds, MSpace::kObject);
        if (!status)
        {
            return status;
        }
    }

    if (context.importMaterials)
    {
        status = AssignFaceSetMaterials(meshFn, faceSetAssignments);
        if (!status)
        {
            return status;
        }
    }

    if (!tangentStrings.empty() && !tangentIndexStrings.empty())
    {
        status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangents", JoinLines(tangentStrings));
        if (!status)
        {
            return status;
        }

        status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangentsIndices", JoinLines(tangentIndexStrings));
        if (!status)
        {
            return status;
        }

        const std::string tangentUvSetName = FindAttributeString(vertexData, "mayaTangentUvSetName");
        if (!tangentUvSetName.empty())
        {
            status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangentUvSetName", tangentUvSetName);
            if (!status)
            {
                return status;
            }
        }
    }

    if (context.importSkin)
    {
        status = ApplySkinning(context, vertexData, meshObject, parent);
        if (!status)
        {
            return status;
        }
    }

    if (context.importDeltaStates)
    {
        status = ApplyDeltaStates(context, document, meshElement, meshObject, parent, points);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

} // namespace dmx_import_impl
