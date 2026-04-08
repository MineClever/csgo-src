#include "DmxExportMeshMaterial.h"
#include "DmxExportInternals.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

namespace dmx_export_impl
{

std::vector<int> BuildPolygonRange(int polygonCount)
{
    std::vector<int> polygonIndices;
    polygonIndices.reserve(static_cast<size_t>(polygonCount));
    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
    {
        polygonIndices.push_back(polygonIndex);
    }
    return polygonIndices;
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

std::vector<int> FilterUncoveredPolygons(const std::vector<int> &polygonIndices, std::vector<bool> &coveredPolygons)
{
    std::vector<int> filteredIndices;
    filteredIndices.reserve(polygonIndices.size());
    for (int polygonIndex : polygonIndices)
    {
        if (polygonIndex < 0 || polygonIndex >= static_cast<int>(coveredPolygons.size()))
        {
            continue;
        }

        if (coveredPolygons[polygonIndex])
        {
            continue;
        }

        coveredPolygons[polygonIndex] = true;
        filteredIndices.push_back(polygonIndex);
    }

    return filteredIndices;
}

std::vector<std::string> BuildFaceValues(
    const std::vector<int> &polygonIndices,
    const std::vector<std::vector<int>> &polygonFaceIndices)
{
    std::vector<std::string> faceValues;
    for (int polygonIndex : polygonIndices)
    {
        if (polygonIndex < 0 || polygonIndex >= static_cast<int>(polygonFaceIndices.size()))
        {
            continue;
        }

        const std::vector<int> &polygonEntries = polygonFaceIndices[polygonIndex];
        if (polygonEntries.empty())
        {
            continue;
        }

        for (int faceVertexIndex : polygonEntries)
        {
            faceValues.push_back(std::to_string(faceVertexIndex));
        }
        faceValues.push_back("-1");
    }

    return faceValues;
}

void AppendFaceSetElement(
    DocumentBuilder &builder,
    const char *faceSetName,
    const std::vector<int> &polygonIndices,
    const std::vector<std::vector<int>> &polygonFaceIndices,
    const MeshMaterialData *materialData,
    bool exportMetadata,
    std::vector<Element *> &faceSetElements)
{
    if (!faceSetName || polygonIndices.empty())
    {
        return;
    }

    std::vector<std::string> faceValues = BuildFaceValues(polygonIndices, polygonFaceIndices);
    if (faceValues.empty())
    {
        return;
    }

    Element *faceSetElement = builder.CreateElement("DmeFaceSet", faceSetName);
    SetAttr(*faceSetElement, "faces", ScalarArrayAttr("int_array", std::move(faceValues)));
    if (exportMetadata && materialData && (!materialData->materialName.empty() || !materialData->shadingGroupName.empty()))
    {
        const std::string matName = materialData->materialName.empty() ? materialData->shadingGroupName : materialData->materialName;
        const std::string sgName = materialData->shadingGroupName.empty() ? materialData->materialName : materialData->shadingGroupName;
        Element *materialElement = builder.CreateElement("DmeMaterial", matName);
        SetAttr(*materialElement, "mtlName", ScalarAttr("string", sgName));
        if (!materialData->shaderName.empty())
        {
            SetAttr(*materialElement, "mayaShaderName", ScalarAttr("string", materialData->shaderName));
        }
        if (!materialData->shaderType.empty())
        {
            SetAttr(*materialElement, "mayaShaderType", ScalarAttr("string", materialData->shaderType));
        }
        if (!materialData->color.empty())
        {
            SetAttr(*materialElement, "mayaColor", ScalarAttr("vector3", materialData->color));
        }
        if (!materialData->transparency.empty())
        {
            SetAttr(*materialElement, "mayaTransparency", ScalarAttr("vector3", materialData->transparency));
        }
        if (!materialData->diffuseTexture.empty())
        {
            SetAttr(*materialElement, "mayaDiffuseTexture", ScalarAttr("string", materialData->diffuseTexture));
        }
        if (!materialData->normalTexture.empty())
        {
            SetAttr(*materialElement, "mayaNormalTexture", ScalarAttr("string", materialData->normalTexture));
        }
        if (!materialData->bumpTexture.empty())
        {
            SetAttr(*materialElement, "mayaBumpTexture", ScalarAttr("string", materialData->bumpTexture));
        }
        SetAttr(*faceSetElement, "material", builder.ElementRef(materialElement));
    }
    faceSetElements.push_back(faceSetElement);
}

MeshMaterialData BuildMaterialData(const MObject &setObject, const std::string &fallbackName)
{
    MeshMaterialData materialData;
    materialData.shadingGroupName = fallbackName;

    MStatus status;
    MFnDependencyNode setNodeFn(setObject, &status);
    if (!status)
    {
        materialData.materialName = fallbackName;
        return materialData;
    }

    MPlug surfaceShaderPlug = setNodeFn.findPlug("surfaceShader", true, &status);
    if (!status)
    {
        materialData.materialName = fallbackName;
        return materialData;
    }

    const MObject shaderObject = FindConnectedSourceNode(surfaceShaderPlug);
    if (shaderObject.isNull())
    {
        materialData.materialName = fallbackName;
        return materialData;
    }

    MFnDependencyNode shaderNodeFn(shaderObject, &status);
    if (!status)
    {
        materialData.materialName = fallbackName;
        return materialData;
    }

    materialData.materialName = shaderNodeFn.name().asChar();
    materialData.shaderName = shaderNodeFn.name().asChar();
    materialData.shaderType = shaderNodeFn.typeName().asChar();

    MPlug colorPlug = shaderNodeFn.findPlug("color", true, &status);
    if (status)
    {
        ReadVector3PlugValue(colorPlug, materialData.color);
        materialData.diffuseTexture = FindTexturePathFromPlug(colorPlug);
    }

    MPlug transparencyPlug = shaderNodeFn.findPlug("transparency", true, &status);
    if (status)
    {
        ReadVector3PlugValue(transparencyPlug, materialData.transparency);
    }

    MPlug normalPlug = shaderNodeFn.findPlug("normalCamera", true, &status);
    if (status)
    {
        materialData.normalTexture = FindTexturePathFromPlug(normalPlug);
        materialData.bumpTexture = materialData.normalTexture;
    }

    return materialData;
}

} // namespace dmx_export_impl
