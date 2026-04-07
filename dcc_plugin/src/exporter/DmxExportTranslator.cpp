#include "DmxExportTranslator.h"
#include "DmxExportTextModel.h"
#include "DmxExportTranslatorTypes.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxTypes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <deque>
#include <fstream>
#include <exception>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSet.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnMatrixData.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MMatrix.h>
#include <maya/MObjectArray.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

#include <Windows.h>

namespace
{
using dmx_export::CloneElement;
using dmx_export::DmxAttribute;
using dmx_export::DmxElement;
using dmx_export::DmxTextBuilder;
using dmx_export::FindAttribute;
using dmx_export::GetElementName;
using dmx_export::MakeElementArrayAttribute;
using dmx_export::MakeInlineElementAttribute;
using dmx_export::MakeScalarArrayAttribute;
using dmx_export::MakeScalarAttribute;
using dmx_export_translator::ExportContext;
using dmx_export_translator::ExportOptions;
using dmx_export_translator::IndexedChannel;
using dmx_export_translator::MeshMaterialData;
std::string FormatVector3(double x, double y, double z);
std::string FormatTimeSeconds(double value);

constexpr int kCurrentBinaryEncoding = 5;

void AppendDebugLog(const char *message)
{
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }

    std::string logPath(tempPath);
    logPath += "maya_dmx_export_debug.log";

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        return;
    }

    logFile << message << "\n";
}

std::vector<double> ParseNumberList(const std::string &text)
{
    std::string normalized = text;
    std::replace_if(
        normalized.begin(),
        normalized.end(),
        [](char c)
        {
            return c == ',' || c == '(' || c == ')' || c == '[' || c == ']';
        },
        ' ');

    std::vector<double> values;
    std::istringstream stream(normalized);
    double value = 0.0;
    while (stream >> value)
    {
        values.push_back(value);
    }

    return values;
}

std::string FormatTimeSeconds(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(4);
    stream << value;
    return stream.str();
}

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
    DmxTextBuilder &builder,
    const char *faceSetName,
    const std::vector<int> &polygonIndices,
    const std::vector<std::vector<int>> &polygonFaceIndices,
    const MeshMaterialData *materialData,
    bool exportMetadata,
    std::vector<DmxElement *> &faceSetElements)
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

    DmxElement *faceSetElement = builder.CreateElement("DmeFaceSet");
    faceSetElement->attributes.push_back(MakeScalarAttribute("name", "string", faceSetName));
    faceSetElement->attributes.push_back(MakeScalarArrayAttribute("faces", "int_array", std::move(faceValues)));
    if (exportMetadata && materialData && (!materialData->materialName.empty() || !materialData->shadingGroupName.empty()))
    {
        DmxElement *materialElement = builder.CreateElement("DmeMaterial");
        materialElement->attributes.push_back(MakeScalarAttribute(
            "name",
            "string",
            materialData->materialName.empty() ? materialData->shadingGroupName : materialData->materialName));
        materialElement->attributes.push_back(MakeScalarAttribute(
            "mtlName",
            "string",
            materialData->shadingGroupName.empty() ? materialData->materialName : materialData->shadingGroupName));
        if (!materialData->shaderName.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaShaderName", "string", materialData->shaderName));
        }
        if (!materialData->shaderType.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaShaderType", "string", materialData->shaderType));
        }
        if (!materialData->color.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaColor", "vector3", materialData->color));
        }
        if (!materialData->transparency.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaTransparency", "vector3", materialData->transparency));
        }
        if (!materialData->diffuseTexture.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaDiffuseTexture", "string", materialData->diffuseTexture));
        }
        if (!materialData->normalTexture.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaNormalTexture", "string", materialData->normalTexture));
        }
        if (!materialData->bumpTexture.empty())
        {
            materialElement->attributes.push_back(MakeScalarAttribute("mayaBumpTexture", "string", materialData->bumpTexture));
        }
        faceSetElement->attributes.push_back(MakeInlineElementAttribute("material", materialElement));
    }
    faceSetElements.push_back(faceSetElement);
}

std::string ReadStringPlugValue(const MPlug &plug)
{
    MString value;
    if (plug.getValue(value) == MS::kSuccess)
    {
        return value.asChar();
    }

    return std::string();
}

std::vector<std::string> SplitLines(const std::string &value)
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

std::string ReadDynamicStringAttribute(const MObject &nodeObject, const char *attributeName)
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

MObject FindPrimaryMeshChild(const MObject &nodeObject)
{
    if (nodeObject.isNull())
    {
        return MObject::kNullObj;
    }

    if (nodeObject.hasFn(MFn::kMesh))
    {
        return nodeObject;
    }

    if (!nodeObject.hasFn(MFn::kTransform))
    {
        return MObject::kNullObj;
    }

    MStatus status;
    MFnDagNode dagNode(nodeObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject child = dagNode.child(childIndex, &status);
        if (!status || child.isNull())
        {
            continue;
        }

        if (child.hasFn(MFn::kMesh))
        {
            MFnDagNode childDagNode(child, &status);
            if (status && !childDagNode.isIntermediateObject())
            {
                return child;
            }
        }
    }

    return MObject::kNullObj;
}

bool TryGetMeshPathFromObject(const MObject &nodeObject, MDagPath &meshPath)
{
    MStatus status;
    MObject meshObject = FindPrimaryMeshChild(nodeObject);
    if (meshObject.isNull())
    {
        return false;
    }

    status = MDagPath::getAPathTo(meshObject, meshPath);
    return status == MS::kSuccess;
}

bool TryRegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MDagPath &targetPath,
    MString &temporaryTransformName)
{
    temporaryTransformName.clear();

    MString command("sculptTarget -e -regenerate true -target ");
    command += static_cast<int>(weightIndex);
    command += " \"";
    command += blendShapeNodeName;
    command += "\"";

    MStringArray result;
    if (MGlobal::executeCommand(command, result, false, false) != MS::kSuccess || result.length() == 0)
    {
        return false;
    }

    temporaryTransformName = result[0];

    MSelectionList selectionList;
    if (selectionList.add(temporaryTransformName) != MS::kSuccess)
    {
        return false;
    }

    MObject temporaryObject;
    if (selectionList.getDependNode(0, temporaryObject) != MS::kSuccess)
    {
        return false;
    }

    return TryGetMeshPathFromObject(temporaryObject, targetPath);
}

bool ReadVector3PlugValue(const MPlug &plug, std::string &formattedValue)
{
    if (plug.numChildren() < 3)
    {
        return false;
    }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (plug.child(0).getValue(x) != MS::kSuccess ||
        plug.child(1).getValue(y) != MS::kSuccess ||
        plug.child(2).getValue(z) != MS::kSuccess)
    {
        return false;
    }

    formattedValue = FormatVector3(x, y, z);
    return true;
}

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

std::string FindTexturePathFromPlug(const MPlug &plug)
{
    const MObject sourceNode = FindConnectedSourceNode(plug);
    if (sourceNode.isNull())
    {
        return std::string();
    }

    MStatus status;
    MFnDependencyNode sourceNodeFn(sourceNode, &status);
    if (!status)
    {
        return std::string();
    }

    if (sourceNodeFn.typeName() == "file")
    {
        MPlug textureNamePlug = sourceNodeFn.findPlug("fileTextureName", true, &status);
        if (status)
        {
            return ReadStringPlugValue(textureNamePlug);
        }
        return std::string();
    }

    if (sourceNodeFn.typeName() == "bump2d" || sourceNodeFn.typeName() == "bump3d")
    {
        MPlug bumpPlug = sourceNodeFn.findPlug("bumpValue", true, &status);
        if (status)
        {
            return FindTexturePathFromPlug(bumpPlug);
        }
    }

    return std::string();
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

void WriteInt32(std::string &bytes, std::int32_t value)
{
    const size_t start = bytes.size();
    bytes.resize(start + sizeof(value));
    std::memcpy(bytes.data() + start, &value, sizeof(value));
}

void WriteUInt8(std::string &bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<char>(value));
}

void WriteFloat32(std::string &bytes, float value)
{
    const size_t start = bytes.size();
    bytes.resize(start + sizeof(value));
    std::memcpy(bytes.data() + start, &value, sizeof(value));
}

void WriteCString(std::string &bytes, const std::string &value)
{
    bytes.append(value);
    bytes.push_back('\0');
}

std::array<std::uint8_t, 16> MakeElementIdBytes(const std::string &id)
{
    std::array<std::uint8_t, 16> bytes{};

    const auto decodeHex = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        return -1;
    };

    if (id.size() == 32)
    {
        bool validHex = true;
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            const int hi = decodeHex(id[i * 2]);
            const int lo = decodeHex(id[i * 2 + 1]);
            if (hi < 0 || lo < 0)
            {
                validHex = false;
                break;
            }
            bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
        }

        if (validHex)
        {
            return bytes;
        }
    }

    const std::uint64_t hashA = std::hash<std::string>{}(id);
    const std::uint64_t hashB = std::hash<std::string>{}(id + "#maya_dmx");
    std::memcpy(bytes.data(), &hashA, sizeof(hashA));
    std::memcpy(bytes.data() + sizeof(hashA), &hashB, sizeof(hashB));
    return bytes;
}

bool IsBinaryExportRequested(const MFileObject &fileObject, const MString &options)
{
    const std::string lowerPath = fileObject.rawFullName().toLowerCase().asChar();
    if (lowerPath.size() >= 5 && lowerPath.substr(lowerPath.size() - 5) == ".dmxb")
    {
        return true;
    }
    if (lowerPath.size() >= 7 && lowerPath.substr(lowerPath.size() - 7) == ".dmxbin")
    {
        return true;
    }

    std::string lowerOptions = options.asChar();
    std::transform(lowerOptions.begin(), lowerOptions.end(), lowerOptions.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowerOptions.find("binary=1") != std::string::npos ||
        lowerOptions.find("binary=true") != std::string::npos ||
        lowerOptions.find("encoding=binary") != std::string::npos;
}

std::unordered_map<std::string, std::string> ParseOptionMap(const MString &options)
{
    std::unordered_map<std::string, std::string> optionMap;
    std::string text = options.asChar();
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find(';', start);
        if (end == std::string::npos)
        {
            end = text.size();
        }

        const std::string pair = text.substr(start, end - start);
        const size_t separator = pair.find('=');
        if (separator != std::string::npos)
        {
            std::string key = pair.substr(0, separator);
            std::string value = pair.substr(separator + 1);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            optionMap[key] = value;
        }

        start = end + 1;
    }
    return optionMap;
}

bool ParseBoolOption(const std::unordered_map<std::string, std::string> &optionMap, const char *key, bool defaultValue)
{
    auto it = optionMap.find(key);
    if (it == optionMap.end())
    {
        return defaultValue;
    }

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "1" || value == "true" || value == "yes";
}

ExportOptions ParseExportOptions(const MFileObject &fileObject, const MString &options)
{
    ExportOptions exportOptions;
    exportOptions.binary = IsBinaryExportRequested(fileObject, options);

    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    exportOptions.exportSkin = ParseBoolOption(optionMap, "exportskin", true);
    exportOptions.exportDeltaStates = ParseBoolOption(optionMap, "exportdeltastates", true);
    exportOptions.exportMetadata = ParseBoolOption(optionMap, "exportmetadata", true);

    auto upAxisIt = optionMap.find("upaxis");
    if (upAxisIt != optionMap.end() && !upAxisIt->second.empty())
    {
        exportOptions.upAxis = upAxisIt->second;
    }

    auto materialRootIt = optionMap.find("materialroot");
    if (materialRootIt != optionMap.end())
    {
        exportOptions.materialRoot = materialRootIt->second;
    }

    return exportOptions;
}

class DmxBinarySerializer
{
public:
    bool Serialize(const DmxElement &root, std::string &output, std::string &errorMessage)
    {
        CollectElements(&root);

        output.clear();
        output += "<!-- dmx encoding binary 5 format dmx 1 -->\n";
        output.push_back('\0');

        WriteInt32(output, static_cast<std::int32_t>(m_strings.size()));
        for (const std::string &value : m_strings)
        {
            WriteCString(output, value);
        }

        WriteInt32(output, static_cast<std::int32_t>(m_elements.size()));
        for (const DmxElement *element : m_elements)
        {
            const auto typeIt = m_stringToIndex.find(element->type);
            const std::string name = GetElementName(*element);
            const auto nameIt = m_stringToIndex.find(name);
            if (typeIt == m_stringToIndex.end() || nameIt == m_stringToIndex.end())
            {
                errorMessage = "Binary DMX export failed: missing string table entry.";
                return false;
            }

            WriteInt32(output, typeIt->second);
            WriteInt32(output, nameIt->second);

            const std::array<std::uint8_t, 16> idBytes = MakeElementIdBytes(element->id);
            output.append(reinterpret_cast<const char *>(idBytes.data()), idBytes.size());
        }

        for (const DmxElement *element : m_elements)
        {
            int attributeCount = 0;
            for (const DmxAttribute &attribute : element->attributes)
            {
                if (attribute.name != "name")
                {
                    ++attributeCount;
                }
            }

            WriteInt32(output, attributeCount);
            for (const DmxAttribute &attribute : element->attributes)
            {
                if (attribute.name == "name")
                {
                    continue;
                }

                auto nameIt = m_stringToIndex.find(attribute.name);
                if (nameIt == m_stringToIndex.end())
                {
                    errorMessage = "Binary DMX export failed: missing attribute name in string table.";
                    return false;
                }

                WriteInt32(output, nameIt->second);
                if (!WriteAttribute(output, attribute, errorMessage))
                {
                    return false;
                }
            }
        }

        return true;
    }

private:
    void AddString(const std::string &value)
    {
        if (m_stringToIndex.find(value) != m_stringToIndex.end())
        {
            return;
        }

        const std::int32_t index = static_cast<std::int32_t>(m_strings.size());
        m_strings.push_back(value);
        m_stringToIndex.emplace(value, index);
    }

    void CollectElements(const DmxElement *element)
    {
        if (!element || m_elementToIndex.find(element) != m_elementToIndex.end())
        {
            return;
        }

        m_elementToIndex[element] = static_cast<std::int32_t>(m_elements.size());
        m_elements.push_back(element);

        AddString(element->type);
        AddString(GetElementName(*element));

        for (const DmxAttribute &attribute : element->attributes)
        {
            AddString(attribute.name);
            if (attribute.kind == DmxAttribute::Kind::Scalar && attribute.type == "string")
            {
                AddString(attribute.value);
            }

            if (attribute.kind == DmxAttribute::Kind::InlineElement && attribute.inlineElement)
            {
                CollectElements(attribute.inlineElement);
            }
            else if (attribute.kind == DmxAttribute::Kind::ElementArray)
            {
                for (DmxElement *child : attribute.elementArray)
                {
                    CollectElements(child);
                }
            }
        }
    }

    bool WriteAttribute(std::string &output, const DmxAttribute &attribute, std::string &errorMessage) const
    {
        switch (attribute.kind)
        {
        case DmxAttribute::Kind::Scalar:
            return WriteScalar(output, attribute, errorMessage);

        case DmxAttribute::Kind::ScalarArray:
            return WriteScalarArray(output, attribute, errorMessage);

        case DmxAttribute::Kind::InlineElement:
        {
            std::uint8_t typeCode = 0;
            simple_dmx::TryGetBinaryTypeCode(simple_dmx::ValueType::Element, typeCode);
            WriteUInt8(output, typeCode);
            return WriteElementReference(output, attribute.inlineElement, errorMessage);
        }

        case DmxAttribute::Kind::ElementArray:
        {
            std::uint8_t typeCode = 0;
            simple_dmx::TryGetBinaryTypeCode(simple_dmx::ValueType::ElementArray, typeCode);
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.elementArray.size()));
            for (DmxElement *child : attribute.elementArray)
            {
                if (!WriteElementReference(output, child, errorMessage))
                {
                    return false;
                }
            }
            return true;
        }
        }

        errorMessage = "Binary DMX export failed: unsupported attribute kind.";
        return false;
    }

    bool WriteScalar(std::string &output, const DmxAttribute &attribute, std::string &errorMessage) const
    {
        const simple_dmx::ValueType valueType = simple_dmx::ValueTypeFromDeclaredType(attribute.type);
        std::uint8_t typeCode = 0;

        if (attribute.type == "string")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            auto it = m_stringToIndex.find(attribute.value);
            if (it == m_stringToIndex.end())
            {
                errorMessage = "Binary DMX export failed: missing string value in string table.";
                return false;
            }
            WriteInt32(output, it->second);
            return true;
        }

        const std::vector<double> values = ParseNumberList(attribute.value);
        if (attribute.type == "int")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            return true;
        }
        if (attribute.type == "float")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            return true;
        }
        if (attribute.type == "bool")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            const bool boolValue = attribute.value == "1" || attribute.value == "true";
            WriteUInt8(output, boolValue ? 1 : 0);
            return true;
        }
        if (attribute.type == "vector2" && values.size() >= 2)
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            return true;
        }
        if (attribute.type == "vector3" && values.size() >= 3)
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            return true;
        }
        if (attribute.type == "vector4" && values.size() >= 4)
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }
        if (attribute.type == "quaternion" && values.size() >= 4)
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }
        if (attribute.type == "time")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(std::round((values.empty() ? 0.0 : values[0]) * 10000.0)));
            return true;
        }

        errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
        return false;
    }

    bool WriteScalarArray(std::string &output, const DmxAttribute &attribute, std::string &errorMessage) const
    {
        auto writeVectorArray = [&](simple_dmx::ValueType valueType, int components) -> bool
        {
            std::uint8_t typeCode = 0;
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                return false;
            }

            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                for (int component = 0; component < components; ++component)
                {
                    const float componentValue =
                        component < static_cast<int>(values.size()) ? static_cast<float>(values[static_cast<size_t>(component)]) : 0.0f;
                    WriteFloat32(output, componentValue);
                }
            }
            return true;
        };

        const simple_dmx::ValueType valueType = simple_dmx::ValueTypeFromDeclaredType(attribute.type);
        std::uint8_t typeCode = 0;

        if (attribute.type == "int_array")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported array attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            }
            return true;
        }
        if (attribute.type == "float_array")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported array attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            }
            return true;
        }
        if (attribute.type == "time_array")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported array attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteInt32(output, static_cast<std::int32_t>(std::round((values.empty() ? 0.0 : values[0]) * 10000.0)));
            }
            return true;
        }
        if (attribute.type == "string_array")
        {
            if (!simple_dmx::TryGetBinaryTypeCode(valueType, typeCode))
            {
                errorMessage = "Binary DMX export failed: unsupported array attribute type '" + attribute.type + "'.";
                return false;
            }
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                WriteCString(output, value);
            }
            return true;
        }
        if (attribute.type == "vector2_array")
        {
            return writeVectorArray(valueType, 2);
        }
        if (attribute.type == "vector3_array")
        {
            return writeVectorArray(valueType, 3);
        }
        if (attribute.type == "vector4_array")
        {
            return writeVectorArray(valueType, 4);
        }
        if (attribute.type == "quaternion_array")
        {
            return writeVectorArray(valueType, 4);
        }

        errorMessage = "Binary DMX export failed: unsupported array attribute type '" + attribute.type + "'.";
        return false;
    }

    bool WriteElementReference(std::string &output, const DmxElement *element, std::string &errorMessage) const
    {
        if (!element)
        {
            WriteInt32(output, -1);
            return true;
        }

        auto it = m_elementToIndex.find(element);
        if (it == m_elementToIndex.end())
        {
            errorMessage = "Binary DMX export failed: element reference was not collected.";
            return false;
        }

        WriteInt32(output, it->second);
        return true;
    }

    std::vector<const DmxElement *> m_elements;
    std::unordered_map<const DmxElement *, std::int32_t> m_elementToIndex;
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, std::int32_t> m_stringToIndex;
};

std::string FormatFloat(double value)
{
    if (!std::isfinite(value))
    {
        value = 0.0;
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str();
}

std::string FormatVector3(double x, double y, double z)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z);
}

std::string FormatVector2(double x, double y)
{
    return FormatFloat(x) + " " + FormatFloat(y);
}

std::string FormatVector4(double x, double y, double z, double w)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z) + " " + FormatFloat(w);
}

std::string FormatQuaternion(double x, double y, double z, double w)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z) + " " + FormatFloat(w);
}

std::string FormatMatrix(const MMatrix &matrix)
{
    std::ostringstream stream;
    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            if (row != 0 || column != 0)
            {
                stream << ' ';
            }
            stream << FormatFloat(matrix[row][column]);
        }
    }
    return stream.str();
}

std::string ReadMatrixPlugValue(const MPlug &plug)
{
    MObject matrixObject;
    if (plug.getValue(matrixObject) != MS::kSuccess || matrixObject.isNull())
    {
        return std::string();
    }

    MStatus status;
    MFnMatrixData matrixDataFn(matrixObject, &status);
    if (!status)
    {
        return std::string();
    }

    return FormatMatrix(matrixDataFn.matrix(&status));
}

bool ShouldExportRoot(const MDagPath &dagPath)
{
    if (!dagPath.isValid())
    {
        return false;
    }

    MFnDagNode dagNode(dagPath);
    if (dagNode.isIntermediateObject())
    {
        return false;
    }

    return dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint);
}

std::string DagPathKey(const MDagPath &dagPath)
{
    return dagPath.fullPathName().asChar();
}

void RegisterDagElementsRecursive(DmxTextBuilder &builder, const MDagPath &dagPath, ExportContext &context)
{
    if (!dagPath.isValid())
    {
        return;
    }

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status || dagNode.isIntermediateObject())
    {
        return;
    }

    if (!(dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint)))
    {
        return;
    }

    const std::string pathKey = DagPathKey(dagPath);
    auto dagElementIt = context.dagElementByPath.find(pathKey);
    if (dagElementIt == context.dagElementByPath.end())
    {
        const std::string elementType = dagPath.hasFn(MFn::kJoint) ? "DmeJoint" : "DmeDag";
        DmxElement *dagElement = builder.CreateElement(elementType);
        context.dagElementByPath[pathKey] = dagElement;
        if (dagPath.hasFn(MFn::kJoint))
        {
            context.jointIndexByPath[pathKey] = static_cast<int>(context.jointElements.size());
            context.jointElements.push_back(dagElement);
        }
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        RegisterDagElementsRecursive(builder, childPath, context);
    }
}

std::vector<MDagPath> CollectExportRoots(MPxFileTranslator::FileAccessMode mode)
{
    std::vector<MDagPath> roots;

    if (mode == MPxFileTranslator::kExportActiveAccessMode)
    {
        MStringArray selectedPaths;
        if (MGlobal::executeCommand("ls -sl -long", selectedPaths) == MS::kSuccess)
        {
            for (unsigned int i = 0; i < selectedPaths.length(); ++i)
            {
                MSelectionList selectionList;
                if (selectionList.add(selectedPaths[i]) != MS::kSuccess)
                {
                    continue;
                }

                MDagPath dagPath;
                if (selectionList.getDagPath(0, dagPath) != MS::kSuccess)
                {
                    continue;
                }

                if (dagPath.hasFn(MFn::kMesh))
                {
                    dagPath.pop();
                }

                if (ShouldExportRoot(dagPath))
                {
                    roots.push_back(dagPath);
                }
            }
        }
    }

    if (roots.empty())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            if (dagIterator.depth() != 1)
            {
                continue;
            }

            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) == MS::kSuccess && ShouldExportRoot(dagPath))
            {
                roots.push_back(dagPath);
            }
        }
    }

    std::vector<MDagPath> filteredRoots;
    for (const MDagPath &candidate : roots)
    {
        bool isDescendant = false;
        for (const MDagPath &other : roots)
        {
            if (candidate == other)
            {
                continue;
            }

            const MString candidatePath = candidate.fullPathName();
            const MString otherPath = other.fullPathName();
            if (candidate.length() > other.length() && candidatePath.indexW(otherPath) == 0)
            {
                isDescendant = true;
                break;
            }
        }

        if (!isDescendant)
        {
            filteredRoots.push_back(candidate);
        }
    }

    return filteredRoots;
}

DmxElement *BuildTransformElement(DmxTextBuilder &builder, const MDagPath &dagPath)
{
    MStatus status;
    MFnTransform transformFn(dagPath, &status);
    if (!status)
    {
        return nullptr;
    }

    const MVector translation = transformFn.translation(MSpace::kTransform, &status);
    if (!status)
    {
        return nullptr;
    }

    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    double qw = 1.0;
    status = transformFn.getRotationQuaternion(qx, qy, qz, qw, MSpace::kTransform);
    if (!status)
    {
        return nullptr;
    }

    DmxElement *transformElement = builder.CreateElement("DmeTransform");
    transformElement->attributes.push_back(MakeScalarAttribute("position", "vector3", FormatVector3(translation.x, translation.y, translation.z)));
    transformElement->attributes.push_back(MakeScalarAttribute("orientation", "quaternion", FormatQuaternion(qx, qy, qz, qw)));
    return transformElement;
}

MObject FindAnimationCurveForPlug(const MPlug &plug)
{
    if (plug.isNull())
    {
        return MObject::kNullObj;
    }

    MStringArray sourceConnections;
    MString command = "listConnections -s true -d false -plugs true \"";
    command += plug.name();
    command += "\"";
    if (MGlobal::executeCommand(command, sourceConnections, false, false) != MS::kSuccess)
    {
        return MObject::kNullObj;
    }

    for (unsigned int connectionIndex = 0; connectionIndex < sourceConnections.length(); ++connectionIndex)
    {
        MSelectionList selectionList;
        if (selectionList.add(sourceConnections[connectionIndex]) != MS::kSuccess)
        {
            continue;
        }

        MPlug sourcePlug;
        if (selectionList.getPlug(0, sourcePlug) != MS::kSuccess)
        {
            continue;
        }

        MStatus status;
        const MObject node = sourcePlug.node(&status);
        if (status && !node.isNull() && node.hasFn(MFn::kAnimCurve))
        {
            return node;
        }
    }

    return MObject::kNullObj;
}

void AppendUniqueTime(std::vector<double> &times, double value)
{
    const auto it = std::lower_bound(times.begin(), times.end(), value);
    if (it != times.end() && std::abs(*it - value) < 1.0e-6)
    {
        return;
    }
    if (it != times.begin())
    {
        const auto previous = it - 1;
        if (std::abs(*previous - value) < 1.0e-6)
        {
            return;
        }
    }
    times.insert(it, value);
}

void AppendCurveTimes(const MObject &curveObject, std::vector<double> &times)
{
    if (curveObject.isNull())
    {
        return;
    }

    MStatus status;
    MFnAnimCurve curveFn(curveObject, &status);
    if (!status)
    {
        return;
    }

    for (unsigned int keyIndex = 0; keyIndex < curveFn.numKeys(&status); ++keyIndex)
    {
        if (!status)
        {
            break;
        }

        AppendUniqueTime(times, curveFn.time(keyIndex, &status).as(MTime::kSeconds));
    }
}

double EvaluateCurveOrValue(const MObject &curveObject, const MPlug &plug, double timeSeconds)
{
    if (!curveObject.isNull())
    {
        MStatus status;
        MFnAnimCurve curveFn(curveObject, &status);
        if (status)
        {
            return curveFn.evaluate(MTime(timeSeconds, MTime::kSeconds), &status);
        }
    }

    double value = 0.0;
    plug.getValue(value);
    return value;
}

#include "DmxExportAnimation.hpp"
#include "DmxExportDeformers.hpp"

DmxElement *BuildMeshElement(DmxTextBuilder &builder, const MDagPath &meshPath, ExportContext &context)
{
    MStatus status;
    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return nullptr;
    }

    MPointArray meshPoints;
    status = meshFn.getPoints(meshPoints, MSpace::kObject);
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

            polygonIndices = FilterUncoveredPolygons(polygonIndices, coveredPolygons);
            const MeshMaterialData materialData = BuildMaterialData(connectedSets[setIndex], setFn.name().asChar());
            AppendFaceSetElement(builder, setFn.name().asChar(), polygonIndices, polygonFaceIndices, &materialData, context.exportMetadata, faceSetElements);
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

DmxElement *BuildDagElement(DmxTextBuilder &builder, const MDagPath &dagPath, ExportContext &context)
{
    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status || dagNode.isIntermediateObject())
    {
        return nullptr;
    }

    const std::string pathKey = DagPathKey(dagPath);
    auto dagElementIt = context.dagElementByPath.find(pathKey);
    if (dagElementIt == context.dagElementByPath.end() || !dagElementIt->second)
    {
        return nullptr;
    }

    const std::string elementType = dagPath.hasFn(MFn::kJoint) ? "DmeJoint" : "DmeDag";
    DmxElement *dagElement = dagElementIt->second;
    dagElement->type = elementType;
    dagElement->attributes.clear();
    dagElement->attributes.push_back(MakeScalarAttribute("name", "string", dagNode.name().asChar()));

    if (DmxElement *transformElement = BuildTransformElement(builder, dagPath))
    {
        context.transformElementByPath[pathKey] = transformElement;
        dagElement->attributes.push_back(MakeInlineElementAttribute("transform", transformElement));
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status)
        {
            continue;
        }

        if (childObject.hasFn(MFn::kMesh))
        {
            MDagPath meshPath = dagPath;
            meshPath.push(childObject);
            MFnDagNode meshDagNode(meshPath, &status);
            if (!status || meshDagNode.isIntermediateObject())
            {
                continue;
            }

            if (DmxElement *meshElement = BuildMeshElement(builder, meshPath, context))
            {
                dagElement->attributes.push_back(MakeInlineElementAttribute("shape", meshElement));
                break;
            }
        }
    }

    std::vector<DmxElement *> childElements;
    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        if (DmxElement *childElement = BuildDagElement(builder, childPath, context))
        {
            childElements.push_back(childElement);
        }
    }

    if (!childElements.empty())
    {
        dagElement->attributes.push_back(MakeElementArrayAttribute("children", childElements));
    }

    return dagElement;
}
}

void *DmxExportTranslator::Create()
{
    return new DmxExportTranslator();
}

bool DmxExportTranslator::haveReadMethod() const
{
    return false;
}

bool DmxExportTranslator::haveWriteMethod() const
{
    return true;
}

MString DmxExportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxExportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxExportTranslator::writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode)
{
    try
    {
        AppendDebugLog("writer: begin");
        const ExportOptions exportOptions = ParseExportOptions(fileObject, options);
        const std::vector<MDagPath> exportRoots = CollectExportRoots(mode);
        AppendDebugLog("writer: collected roots");
        if (exportRoots.empty())
        {
            AppendDebugLog("writer: no roots");
            return maya_dmx::ReportError("maya_dmx: nothing to export.");
        }

        DmxTextBuilder builder;
        ExportContext context;
        context.exportSkin = exportOptions.exportSkin;
        context.exportDeltaStates = exportOptions.exportDeltaStates;
        context.exportMetadata = exportOptions.exportMetadata;
        context.materialRoot = exportOptions.materialRoot;
        DmxElement *modelElement = builder.CreateElement("DmeModel");
        modelElement->attributes.push_back(MakeScalarAttribute("name", "string", "maya_export"));
        modelElement->attributes.push_back(MakeScalarAttribute("upAxis", "string", exportOptions.upAxis));
        if (exportOptions.exportMetadata && !exportOptions.materialRoot.empty())
        {
            modelElement->attributes.push_back(MakeScalarAttribute("mayaMaterialRoot", "string", exportOptions.materialRoot));
        }

        std::vector<DmxElement *> rootChildren;
        for (const MDagPath &rootPath : exportRoots)
        {
            RegisterDagElementsRecursive(builder, rootPath, context);
        }
        for (const MDagPath &rootPath : exportRoots)
        {
            if (DmxElement *child = BuildDagElement(builder, rootPath, context))
            {
                rootChildren.push_back(child);
            }
        }
        AppendDebugLog("writer: built dag elements");

        if (!rootChildren.empty())
        {
            modelElement->attributes.push_back(MakeElementArrayAttribute("children", rootChildren));
        }
        if (!context.jointElements.empty())
        {
            modelElement->attributes.push_back(MakeElementArrayAttribute("jointList", context.jointElements));
        }
        if (DmxElement *animationListElement = BuildAnimationListElement(builder, exportRoots, context))
        {
            modelElement->attributes.push_back(MakeInlineElementAttribute("animationList", animationListElement));
        }

        const bool binaryExport = exportOptions.binary;

        std::string serialized;
        std::string serializeError;
        if (binaryExport)
        {
            DmxBinarySerializer binarySerializer;
            if (!binarySerializer.Serialize(*modelElement, serialized, serializeError))
            {
                AppendDebugLog("writer: binary serialize failed");
                return maya_dmx::ReportError(serializeError.c_str());
            }
        }
        else
        {
            serialized = builder.Serialize(*modelElement);
        }
        AppendDebugLog("writer: serialized");

        std::ofstream output(fileObject.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to open output file ") + fileObject.rawFullName());
        }

        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.close();
        AppendDebugLog("writer: wrote file");

        if (!output)
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to write output file ") + fileObject.rawFullName());
        }

        return maya_dmx::ReportInfo(
            MString(binaryExport ? "maya_dmx: exported binary DMX to " : "maya_dmx: exported text DMX to ") + fileObject.rawFullName());
    }
    catch (const std::exception &exception)
    {
        AppendDebugLog("writer: std exception");
        return maya_dmx::ReportError(MString("maya_dmx: export failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        AppendDebugLog("writer: unknown exception");
        return maya_dmx::ReportError("maya_dmx: export failed with an unknown host exception.");
    }
}
