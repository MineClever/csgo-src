#include "DmxExportMesh.h"

#include "DmxExportDeformers.h"
#include "DmxExportMeshMaterial.h"

#include <common/MaterialExportUtils.h>

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

Element *BuildMeshElement(DocumentBuilder &builder, const MDagPath &meshPath, ExportContext &context, const MDagPath *bindShapeMeshPath)
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
        const MVector correctedPoint = dcc_export_transform::ApplyToPoint(
            context.transformPolicy,
            MVector(meshPoints[i].x, meshPoints[i].y, meshPoints[i].z));
        positions.push_back(FormatVector3(correctedPoint.x, correctedPoint.y, correctedPoint.z));
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
                const MVector correctedNormal = dcc_export_transform::ApplyToDirection(context.transformPolicy, normal);
                const std::string normalKey = FormatVector3(correctedNormal.x, correctedNormal.y, correctedNormal.z);
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
                const MVector correctedTangent = dcc_export_transform::ApplyToDirection(context.transformPolicy, tangent);
                const std::string tangentKey = FormatVector4(correctedTangent.x, correctedTangent.y, correctedTangent.z, 1.0);
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

    Element *vertexDataElement = builder.CreateElement("DmeVertexData", "bind");
    SetAttr(*vertexDataElement, "flipVCoordinates", ScalarAttr("bool", "0"));
    SetAttr(*vertexDataElement, "vertexFormat", ScalarArrayAttr("string_array", std::move(vertexFormat)));
    SetAttr(*vertexDataElement, "positions", ScalarArrayAttr("vector3_array", positions));
    SetAttr(*vertexDataElement, "positionsIndices", ScalarArrayAttr("int_array", positionsIndices));

    if (!normals.empty() && normalsIndices.size() == positionsIndices.size())
    {
        SetAttr(*vertexDataElement, "normals", ScalarArrayAttr("vector3_array", normals));
        SetAttr(*vertexDataElement, "normalsIndices", ScalarArrayAttr("int_array", normalsIndices));
    }

    std::vector<std::string> exportedUvSetNames;
    for (unsigned int uvSetIndex = 0; uvSetIndex < uvChannels.size(); ++uvSetIndex)
    {
        const IndexedChannel &uvChannel = uvChannels[uvSetIndex];
        if (uvChannel.values.empty() || uvChannel.indices.size() != positionsIndices.size())
        {
            continue;
        }

        SetAttr(*vertexDataElement, uvChannel.valueAttributeName, ScalarArrayAttr("vector2_array", uvChannel.values));
        SetAttr(*vertexDataElement, uvChannel.indexAttributeName, ScalarArrayAttr("int_array", uvChannel.indices));
        exportedUvSetNames.push_back(uvSetNames[uvSetIndex].asChar());
    }

    if (context.exportMetadata && !exportedUvSetNames.empty())
    {
        SetAttr(*vertexDataElement, "mayaUvSetNames", ScalarArrayAttr("string_array", exportedUvSetNames));
    }

    if (useStoredTangents)
    {
        SetAttr(*vertexDataElement, "tangents", ScalarArrayAttr("vector4_array", storedTangents));
        SetAttr(*vertexDataElement, "tangentsIndices", ScalarArrayAttr("int_array", storedTangentIndices));
        if (context.exportMetadata && !storedTangentUvSetName.empty())
        {
            SetAttr(*vertexDataElement, "mayaTangentUvSetName", ScalarAttr("string", storedTangentUvSetName));
        }
    }
    else if (!tangentChannel.values.empty() && tangentChannel.indices.size() == positionsIndices.size())
    {
        SetAttr(*vertexDataElement, "tangents", ScalarArrayAttr("vector4_array", tangentChannel.values));
        SetAttr(*vertexDataElement, "tangentsIndices", ScalarArrayAttr("int_array", tangentChannel.indices));
        if (context.exportMetadata && uvSetNames.length() > 0)
        {
            SetAttr(*vertexDataElement, "mayaTangentUvSetName", ScalarAttr("string", uvSetNames[0].asChar()));
        }
    }

    if (context.exportSkin)
    {
        AppendSkinningData(meshPath, *vertexDataElement, context);
    }

    std::vector<Element *> deltaStateElements;
    if (context.exportDeltaStates)
    {
        AppendBlendShapeDeltaStates(builder, meshPath, meshPoints, context, deltaStateElements);
    }

    std::vector<Element *> faceSetElements;
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
    if (dcc_material_export::GetMeshSetAssignments(meshFn, meshPath.instanceNumber(), connectedSets, setComponents) &&
        connectedSets.length() > 0 && connectedSets.length() == setComponents.length())
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

            std::vector<int> polygonIndices = dcc_material_export::ExtractPolygonIndices(componentObject);
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

        // Build polygon-to-set-index mapping so we can walk polygons in sequential order.
        // This is needed to handle face sets whose polygon indices are non-contiguous
        // (e.g. a default set that covers [0-2245] + [2286-2716] with n-gon sets in between).
        // Without interleaving, the large set would be emitted as one block and on re-import
        // the polygon ordering would shift, breaking topology roundtrip.
        std::vector<int> polyToSetIndex(polygonFaceIndices.size(), -1);
        for (int setIdx = 0; setIdx < static_cast<int>(perFaceComponentSets.size()); ++setIdx)
        {
            for (int polyIdx : perFaceComponentSets[setIdx].polygonIndices)
            {
                if (polyIdx >= 0 && polyIdx < static_cast<int>(polyToSetIndex.size()))
                {
                    polyToSetIndex[polyIdx] = setIdx;
                }
            }
        }

        // Walk polygons in sequential order and emit contiguous runs into their face sets.
        // Non-contiguous face sets are split into multiple DMX face set elements so that
        // the re-imported polygon order matches the original topology.
        int currentSetIdx = -1;
        std::vector<int> currentRun;

        auto flushRun = [&]()
        {
            if (currentRun.empty() || currentSetIdx < 0)
            {
                currentRun.clear();
                return;
            }
            SetMembership &membership = perFaceComponentSets[currentSetIdx];
            std::vector<int> filteredRun = FilterUncoveredPolygons(currentRun, coveredPolygons);
            if (!filteredRun.empty())
            {
                AppendFaceSetElement(builder, membership.name.c_str(), filteredRun, polygonFaceIndices,
                                     &membership.materialData, context.exportMetadata, faceSetElements);
            }
            currentRun.clear();
        };

        for (int polyIdx = 0; polyIdx < static_cast<int>(polygonFaceIndices.size()); ++polyIdx)
        {
            const int setIdx = polyToSetIndex[polyIdx];
            if (setIdx < 0)
            {
                flushRun();
                currentSetIdx = -1;
                continue;
            }
            if (setIdx != currentSetIdx)
            {
                flushRun();
                currentSetIdx = setIdx;
            }
            currentRun.push_back(polyIdx);
        }
        flushRun();

        for (const SetMembership &membership : deferredWholeObjectSets)
        {
            std::vector<int> polygonIndices = FilterUncoveredPolygons(membership.polygonIndices, coveredPolygons);
            AppendFaceSetElement(builder, membership.name.c_str(), polygonIndices, polygonFaceIndices,
                                 &membership.materialData, context.exportMetadata, faceSetElements);
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

    Element *meshElement = builder.CreateElement("DmeMesh", meshFn.name().asChar());
    SetAttr(*meshElement, "bindState", builder.ElementRef(vertexDataElement));
    Element *baseStateElement = CloneElement(builder, *vertexDataElement);
    SetAttr(*meshElement, "baseStates", builder.ElementRefArray({baseStateElement}));
    Element *currentStateElement = builder.CreateElement("DmeVertexData", "current");
    SetAttr(*currentStateElement, "flipVCoordinates", ScalarAttr("bool", "0"));
    SetAttr(*currentStateElement, "vertexFormat", ScalarArrayAttr("string_array", {"positions"}));
    SetAttr(*currentStateElement, "positions", ScalarArrayAttr("vector3_array", positions));
    SetAttr(*currentStateElement, "positionsIndices", ScalarArrayAttr("int_array", positionsIndices));
    SetAttr(*meshElement, "currentState", builder.ElementRef(currentStateElement));
    if (!deltaStateElements.empty())
    {
        SetAttr(*meshElement, "deltaStates", builder.ElementRefArray(deltaStateElements));
    }
    SetAttr(*meshElement, "faceSets", builder.ElementRefArray(faceSetElements));
    return meshElement;
}

} // namespace dmx_export_impl
