#include "DmxExportMesh.h"

#include "DmxExportDeformers.h"
#include "DmxExportMeshMaterial.h"

#include <algorithm>
#include <climits>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObjectArray.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MVector.h>

namespace dmx_export_impl
{

static std::vector<std::string> SplitLines(const std::string &value)
{
    std::vector<std::string> lines;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty())
        {
            lines.push_back(line);
        }
    }
    return lines;
}

static std::string ReadDynamicStringAttribute(const MObject &nodeObject, const char *attributeName)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return std::string();
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status)
    {
        return std::string();
    }

    return ReadStringPlugValue(attributePlug);
}

DmxElement *BuildMeshElement(DmxTextBuilder &builder, const MDagPath &meshPath, ExportContext &context, const MDagPath *bindShapeMeshPath)
{
    MStatus status;
    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return nullptr;
    }

    MPointArray meshPoints;
    if (bindShapeMeshPath)
    {
        MFnMesh bindShapeMeshFn(*bindShapeMeshPath, &status);
        if (status)
        {
            status = bindShapeMeshFn.getPoints(meshPoints, MSpace::kObject);
        }
        if (!status)
        {
            status = meshFn.getPoints(meshPoints, MSpace::kObject);
        }
    }
    else
    {
        status = meshFn.getPoints(meshPoints, MSpace::kObject);
    }
    if (!status)
    {
        return nullptr;
    }

    std::vector<std::string> positions;
    positions.reserve(meshPoints.length());
    for (unsigned int i = 0; i < meshPoints.length(); ++i)
    {
        positions.push_back(FormatVector3(meshPoints[i].x, meshPoints[i].y, meshPoints[i].z));
    }

    std::vector<std::string> positionsIndices;
    std::vector<std::string> normals;
    std::vector<std::string> normalsIndices;
    std::vector<std::vector<int>> polygonFaceIndices;
    std::unordered_map<std::string, int> normalMap;

    MStringArray uvSetNames;
    std::vector<IndexedChannel> uvChannels;
    if (meshFn.getUVSetNames(uvSetNames) == MS::kSuccess)
    {
        for (unsigned int uvSetIndex = 0; uvSetIndex < uvSetNames.length(); ++uvSetIndex)
        {
            IndexedChannel uvChannel;
            uvChannel.formatName = uvSetIndex == 0 ? "textureCoordinates" : "texcoord$" + std::to_string(uvSetIndex);
            uvChannel.valueAttributeName = uvChannel.formatName;
            uvChannel.indexAttributeName = uvChannel.formatName + "Indices";
            uvChannels.push_back(std::move(uvChannel));
        }
    }

    IndexedChannel tangentChannel;
    tangentChannel.formatName = "tangents";
    tangentChannel.valueAttributeName = "tangents";
    tangentChannel.indexAttributeName = "tangentsIndices";

    const std::vector<std::string> storedTangents = SplitLines(ReadDynamicStringAttribute(meshPath.node(), "mayaDmxTangents"));
    const std::vector<std::string> storedTangentIndices = SplitLines(ReadDynamicStringAttribute(meshPath.node(), "mayaDmxTangentsIndices"));
    const std::string storedTangentUvSetName = ReadDynamicStringAttribute(meshPath.node(), "mayaDmxTangentUvSetName");
    const bool hasStoredTangents = !storedTangents.empty() && !storedTangentIndices.empty();

    MItMeshPolygon polygonIt(meshPath);
    for (; !polygonIt.isDone(); polygonIt.next())
    {
        MIntArray polygonVertices;
        polygonIt.getVertices(polygonVertices);
        std::vector<int> polygonEntries;
        polygonEntries.reserve(polygonVertices.length());

        for (unsigned int localVertex = 0; localVertex < polygonVertices.length(); ++localVertex)
        {
            positionsIndices.push_back(std::to_string(polygonVertices[localVertex]));
            polygonEntries.push_back(static_cast<int>(positionsIndices.size() - 1));

            MVector normal;
            if (polygonIt.getNormal(localVertex, normal, MSpace::kObject) == MS::kSuccess)
            {
                const std::string normalKey = FormatVector3(normal.x, normal.y, normal.z);
                auto [normalIt, inserted] = normalMap.emplace(normalKey, static_cast<int>(normals.size()));
                if (inserted)
                {
                    normals.push_back(normalKey);
                }
                normalsIndices.push_back(std::to_string(normalIt->second));
            }

            for (unsigned int uvSetIndex = 0; uvSetIndex < uvChannels.size(); ++uvSetIndex)
            {
                float2 uv{};
                if (polygonIt.hasUVs(uvSetNames[uvSetIndex]) &&
                    polygonIt.getUV(localVertex, uv, &uvSetNames[uvSetIndex]) == MS::kSuccess)
                {
                    const std::string uvKey = FormatVector2(uv[0], uv[1]);
                    auto [uvIt, inserted] = uvChannels[uvSetIndex].valueMap.emplace(
                        uvKey,
                        static_cast<int>(uvChannels[uvSetIndex].values.size()));
                    if (inserted)
                    {
                        uvChannels[uvSetIndex].values.push_back(uvKey);
                    }
                    uvChannels[uvSetIndex].indices.push_back(std::to_string(uvIt->second));
                }
            }

            if (!uvSetNames.length() || hasStoredTangents)
            {
                continue;
            }

            MVector tangent;
            if (meshFn.getFaceVertexTangent(
                    polygonIt.index(),
                    polygonVertices[localVertex],
                    tangent,
                    MSpace::kObject,
                    &uvSetNames[0]) == MS::kSuccess)
            {
                const std::string tangentKey = FormatVector4(tangent.x, tangent.y, tangent.z, 1.0);
                auto [tangentIt, inserted] = tangentChannel.valueMap.emplace(
                    tangentKey,
                    static_cast<int>(tangentChannel.values.size()));
                if (inserted)
                {
                    tangentChannel.values.push_back(tangentKey);
                }
                tangentChannel.indices.push_back(std::to_string(tangentIt->second));
            }
        }

        polygonFaceIndices.push_back(std::move(polygonEntries));
    }

    std::vector<std::string> vertexFormat = {"positions"};
    if (!normals.empty() && normalsIndices.size() == positionsIndices.size())
    {
        vertexFormat.push_back("normals");
    }
    for (const IndexedChannel &uvChannel : uvChannels)
    {
        if (!uvChannel.values.empty() && uvChannel.indices.size() == positionsIndices.size())
        {
            vertexFormat.push_back(uvChannel.formatName);
        }
    }
    const bool useStoredTangents = hasStoredTangents && storedTangentIndices.size() == positionsIndices.size();
    if (!tangentChannel.values.empty() && tangentChannel.indices.size() == positionsIndices.size())
    {
        vertexFormat.push_back("tangents");
    }
    else if (useStoredTangents)
    {
        vertexFormat.push_back("tangents");
    }

    DmxElement *vertexDataElement = builder.CreateElement("DmeVertexData");
    vertexDataElement->attributes.push_back(MakeScalarAttribute("name", "string", "bind"));
    vertexDataElement->attributes.push_back(MakeScalarAttribute("flipVCoordinates", "bool", "0"));
    vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("vertexFormat", "string_array", std::move(vertexFormat)));
    vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("positions", "vector3_array", positions));
    vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("positionsIndices", "int_array", positionsIndices));

    if (!normals.empty() && normalsIndices.size() == positionsIndices.size())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("normals", "vector3_array", normals));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("normalsIndices", "int_array", normalsIndices));
    }

    std::vector<std::string> exportedUvSetNames;
    for (unsigned int uvSetIndex = 0; uvSetIndex < uvChannels.size(); ++uvSetIndex)
    {
        const IndexedChannel &uvChannel = uvChannels[uvSetIndex];
        if (uvChannel.values.empty() || uvChannel.indices.size() != positionsIndices.size())
        {
            continue;
        }

        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute(
            uvChannel.valueAttributeName,
            "vector2_array",
            uvChannel.values));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute(
            uvChannel.indexAttributeName,
            "int_array",
            uvChannel.indices));
        exportedUvSetNames.push_back(uvSetNames[uvSetIndex].asChar());
    }

    if (context.exportMetadata && !exportedUvSetNames.empty())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("mayaUvSetNames", "string_array", exportedUvSetNames));
    }

    if (useStoredTangents)
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangents", "vector4_array", storedTangents));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangentsIndices", "int_array", storedTangentIndices));
        if (context.exportMetadata && !storedTangentUvSetName.empty())
        {
            vertexDataElement->attributes.push_back(MakeScalarAttribute("mayaTangentUvSetName", "string", storedTangentUvSetName));
        }
    }
    else if (!tangentChannel.values.empty() && tangentChannel.indices.size() == positionsIndices.size())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangents", "vector4_array", tangentChannel.values));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangentsIndices", "int_array", tangentChannel.indices));
        if (context.exportMetadata && uvSetNames.length() > 0)
        {
            vertexDataElement->attributes.push_back(MakeScalarAttribute("mayaTangentUvSetName", "string", uvSetNames[0].asChar()));
        }
    }

    if (context.exportSkin)
    {
        AppendSkinningData(meshPath, *vertexDataElement, context);
    }

    std::vector<DmxElement *> deltaStateElements;
    if (context.exportDeltaStates)
    {
        AppendBlendShapeDeltaStates(builder, meshPath, meshPoints, context, deltaStateElements);
    }

    std::vector<DmxElement *> faceSetElements;
    std::vector<bool> coveredPolygons(polygonFaceIndices.size(), false);
    struct SetMembership
    {
        std::string name;
        std::vector<int> polygonIndices;
        MeshMaterialData materialData;
    };
    std::vector<SetMembership> perFaceComponentSets;
    std::vector<SetMembership> deferredWholeObjectSets;
    MObjectArray connectedSets;
    MObjectArray setComponents;
    status = meshFn.getConnectedSetsAndMembers(meshPath.instanceNumber(), connectedSets, setComponents, true);
    if (status && connectedSets.length() > 0 && connectedSets.length() == setComponents.length())
    {
        for (unsigned int setIndex = 0; setIndex < connectedSets.length(); ++setIndex)
        {
            MFnSet setFn(connectedSets[setIndex], &status);
            if (!status)
            {
                continue;
            }

            MObject componentObject = setComponents[setIndex];
            if (componentObject.isNull())
            {
                deferredWholeObjectSets.push_back(
                    SetMembership{
                        setFn.name().asChar(),
                        BuildPolygonRange(static_cast<int>(polygonFaceIndices.size())),
                        BuildMaterialData(connectedSets[setIndex], setFn.name().asChar())});
                continue;
            }

            std::vector<int> polygonIndices = ExtractPolygonIndices(componentObject);
            if (polygonIndices.empty())
            {
                continue;
            }

            std::sort(polygonIndices.begin(), polygonIndices.end());
            perFaceComponentSets.push_back(
                SetMembership{
                    setFn.name().asChar(),
                    std::move(polygonIndices),
                    BuildMaterialData(connectedSets[setIndex], setFn.name().asChar())});
        }

        // Sort per-face sets by minimum polygon index so the exported face set order matches
        // MItMeshPolygon iteration order. This ensures roundtrip polygon ordering is stable
        // regardless of the order getConnectedSetsAndMembers() returns shading groups.
        std::sort(
            perFaceComponentSets.begin(),
            perFaceComponentSets.end(),
            [](const SetMembership &a, const SetMembership &b)
            {
                const int minA = a.polygonIndices.empty() ? INT_MAX : *std::min_element(a.polygonIndices.begin(), a.polygonIndices.end());
                const int minB = b.polygonIndices.empty() ? INT_MAX : *std::min_element(b.polygonIndices.begin(), b.polygonIndices.end());
                return minA < minB;
            });

        for (SetMembership &membership : perFaceComponentSets)
        {
            std::vector<int> filteredIndices = FilterUncoveredPolygons(membership.polygonIndices, coveredPolygons);
            AppendFaceSetElement(builder, membership.name.c_str(), filteredIndices, polygonFaceIndices, &membership.materialData, context.exportMetadata, faceSetElements);
        }

        for (const SetMembership &membership : deferredWholeObjectSets)
        {
            std::vector<int> polygonIndices = FilterUncoveredPolygons(membership.polygonIndices, coveredPolygons);
            AppendFaceSetElement(builder, membership.name.c_str(), polygonIndices, polygonFaceIndices, &membership.materialData, context.exportMetadata, faceSetElements);
        }
    }

    std::vector<int> uncoveredPolygons;
    uncoveredPolygons.reserve(coveredPolygons.size());
    for (int polygonIndex = 0; polygonIndex < static_cast<int>(coveredPolygons.size()); ++polygonIndex)
    {
        if (!coveredPolygons[polygonIndex])
        {
            uncoveredPolygons.push_back(polygonIndex);
        }
    }

    if (!uncoveredPolygons.empty() || faceSetElements.empty())
    {
        if (uncoveredPolygons.empty())
        {
            uncoveredPolygons = BuildPolygonRange(static_cast<int>(polygonFaceIndices.size()));
        }
        AppendFaceSetElement(builder, "default_faces", uncoveredPolygons, polygonFaceIndices, nullptr, context.exportMetadata, faceSetElements);
    }

    DmxElement *meshElement = builder.CreateElement("DmeMesh");
    meshElement->attributes.push_back(MakeScalarAttribute("name", "string", meshFn.name().asChar()));
    meshElement->attributes.push_back(MakeInlineElementAttribute("bindState", vertexDataElement));
    DmxElement *baseStateElement = CloneElement(builder, *vertexDataElement);
    meshElement->attributes.push_back(MakeElementArrayAttribute("baseStates", {baseStateElement}));
    DmxElement *currentStateElement = CloneElement(builder, *vertexDataElement);
    currentStateElement->attributes.clear();
    currentStateElement->attributes.push_back(MakeScalarAttribute("name", "string", "current"));
    currentStateElement->attributes.push_back(MakeScalarAttribute("flipVCoordinates", "bool", "0"));
    currentStateElement->attributes.push_back(MakeScalarArrayAttribute("vertexFormat", "string_array", {"positions"}));
    currentStateElement->attributes.push_back(MakeScalarArrayAttribute("positions", "vector3_array", positions));
    currentStateElement->attributes.push_back(MakeScalarArrayAttribute("positionsIndices", "int_array", positionsIndices));
    meshElement->attributes.push_back(MakeInlineElementAttribute("currentState", currentStateElement));
    if (!deltaStateElements.empty())
    {
        meshElement->attributes.push_back(MakeElementArrayAttribute("deltaStates", deltaStateElements));
    }
    meshElement->attributes.push_back(MakeElementArrayAttribute("faceSets", faceSetElements));
    return meshElement;
}

} // namespace dmx_export_impl
