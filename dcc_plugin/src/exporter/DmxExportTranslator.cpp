#include "DmxExportTranslator.h"

#include "../common/MayaDmxCommon.h"

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
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
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
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <maya/MVector.h>

#include <Windows.h>

namespace
{
struct DmxElement;

struct DmxAttribute
{
    enum class Kind
    {
        Scalar,
        ScalarArray,
        InlineElement,
        ElementArray,
    };

    Kind kind = Kind::Scalar;
    std::string name;
    std::string type;
    std::string value;
    std::vector<std::string> scalarArray;
    DmxElement *inlineElement = nullptr;
    std::vector<DmxElement *> elementArray;
};

struct DmxElement
{
    std::string type;
    std::string id;
    std::vector<DmxAttribute> attributes;
};

struct ExportContext
{
    std::vector<DmxElement *> jointElements;
    std::unordered_map<std::string, int> jointIndexByPath;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    std::string materialRoot;
};

struct ExportOptions
{
    bool binary = false;
    bool exportSkin = true;
    bool exportDeltaStates = true;
    std::string upAxis = "Y";
    std::string materialRoot;
};

struct IndexedChannel
{
    std::string formatName;
    std::string valueAttributeName;
    std::string indexAttributeName;
    std::vector<std::string> values;
    std::vector<std::string> indices;
    std::unordered_map<std::string, int> valueMap;
};

struct MeshMaterialData
{
    std::string materialName;
    std::string shadingGroupName;
    std::string shaderName;
    std::string shaderType;
    std::string color;
    std::string transparency;
    std::string diffuseTexture;
    std::string normalTexture;
    std::string bumpTexture;
};

DmxAttribute MakeScalarAttribute(const std::string &name, const std::string &type, const std::string &value);
DmxAttribute MakeScalarArrayAttribute(const std::string &name, const std::string &type, std::vector<std::string> values);
DmxAttribute MakeInlineElementAttribute(const std::string &name, DmxElement *element);
DmxAttribute MakeElementArrayAttribute(const std::string &name, const std::vector<DmxElement *> &elements);
std::string FormatVector3(double x, double y, double z);

class DmxTextBuilder
{
public:
    DmxElement *CreateElement(const std::string &type)
    {
        m_elements.push_back({});
        DmxElement &element = m_elements.back();
        element.type = type;
        element.id = "id_" + std::to_string(++m_nextId);
        return &element;
    }

    std::string Serialize(const DmxElement &root) const
    {
        std::ostringstream stream;
        stream << "<!-- dmx encoding keyvalues2 1 format model 1 -->\n";
        WriteElement(stream, root, 0);
        return stream.str();
    }

private:
    static std::string Indent(int level)
    {
        return std::string(level * 4, ' ');
    }

    static void WriteQuoted(std::ostringstream &stream, const std::string &value)
    {
        stream << '"';
        for (char ch : value)
        {
            if (ch == '"' || ch == '\\')
            {
                stream << '\\';
            }
            stream << ch;
        }
        stream << '"';
    }

    static void WriteElement(std::ostringstream &stream, const DmxElement &element, int indentLevel)
    {
        stream << Indent(indentLevel);
        WriteQuoted(stream, element.type);
        stream << "\n";
        WriteElementBody(stream, element, indentLevel);
    }

    static void WriteElementBody(std::ostringstream &stream, const DmxElement &element, int indentLevel)
    {
        stream << Indent(indentLevel) << "{\n";

        stream << Indent(indentLevel + 1);
        WriteQuoted(stream, "id");
        stream << " ";
        WriteQuoted(stream, "elementid");
        stream << " ";
        WriteQuoted(stream, element.id);
        stream << "\n";

        for (const DmxAttribute &attribute : element.attributes)
        {
            WriteAttribute(stream, attribute, indentLevel + 1);
        }

        stream << Indent(indentLevel) << "}\n";
    }

    static void WriteAttribute(std::ostringstream &stream, const DmxAttribute &attribute, int indentLevel)
    {
        stream << Indent(indentLevel);
        WriteQuoted(stream, attribute.name);
        stream << " ";

        switch (attribute.kind)
        {
        case DmxAttribute::Kind::Scalar:
            WriteQuoted(stream, attribute.type);
            stream << " ";
            WriteQuoted(stream, attribute.value);
            stream << "\n";
            break;

        case DmxAttribute::Kind::ScalarArray:
            WriteQuoted(stream, attribute.type);
            stream << "\n" << Indent(indentLevel) << "[\n";
            for (size_t i = 0; i < attribute.scalarArray.size(); ++i)
            {
                stream << Indent(indentLevel + 1);
                WriteQuoted(stream, attribute.scalarArray[i]);
                if (i + 1 < attribute.scalarArray.size())
                {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << Indent(indentLevel) << "]\n";
            break;

        case DmxAttribute::Kind::InlineElement:
            WriteQuoted(stream, attribute.inlineElement->type);
            stream << "\n";
            WriteElementBody(stream, *attribute.inlineElement, indentLevel);
            break;

        case DmxAttribute::Kind::ElementArray:
            WriteQuoted(stream, "element_array");
            stream << "\n" << Indent(indentLevel) << "[\n";
            for (size_t i = 0; i < attribute.elementArray.size(); ++i)
            {
                WriteElement(stream, *attribute.elementArray[i], indentLevel + 1);
                if (i + 1 < attribute.elementArray.size())
                {
                    stream << Indent(indentLevel + 1) << ",\n";
                }
            }
            stream << Indent(indentLevel) << "]\n";
            break;
        }
    }

    std::deque<DmxElement> m_elements;
    int m_nextId = 0;
};

constexpr std::uint8_t kAttributeElement = 1;
constexpr std::uint8_t kAttributeInt = 2;
constexpr std::uint8_t kAttributeFloat = 3;
constexpr std::uint8_t kAttributeBool = 4;
constexpr std::uint8_t kAttributeString = 5;
constexpr std::uint8_t kAttributeVector2 = 9;
constexpr std::uint8_t kAttributeVector3 = 10;
constexpr std::uint8_t kAttributeVector4 = 11;
constexpr std::uint8_t kAttributeQuaternion = 13;
constexpr std::uint8_t kAttributeElementArray = 15;
constexpr std::uint8_t kAttributeIntArray = 16;
constexpr std::uint8_t kAttributeFloatArray = 17;
constexpr std::uint8_t kAttributeStringArray = 19;
constexpr std::uint8_t kAttributeVector2Array = 23;
constexpr std::uint8_t kAttributeVector3Array = 24;
constexpr std::uint8_t kAttributeVector4Array = 25;
constexpr std::uint8_t kAttributeQuaternionArray = 27;
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

const DmxAttribute *FindAttribute(const DmxElement &element, const char *attributeName)
{
    for (const DmxAttribute &attribute : element.attributes)
    {
        if (attribute.name == attributeName)
        {
            return &attribute;
        }
    }
    return nullptr;
}

std::string GetElementName(const DmxElement &element)
{
    if (const DmxAttribute *nameAttribute = FindAttribute(element, "name"))
    {
        return nameAttribute->value;
    }
    return std::string();
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
    if (materialData && (!materialData->materialName.empty() || !materialData->shadingGroupName.empty()))
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

DmxElement *CloneElement(DmxTextBuilder &builder, const DmxElement &source)
{
    DmxElement *clone = builder.CreateElement(source.type);
    clone->attributes = source.attributes;
    return clone;
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
            WriteUInt8(output, kAttributeElement);
            return WriteElementReference(output, attribute.inlineElement, errorMessage);

        case DmxAttribute::Kind::ElementArray:
            WriteUInt8(output, kAttributeElementArray);
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

        errorMessage = "Binary DMX export failed: unsupported attribute kind.";
        return false;
    }

    bool WriteScalar(std::string &output, const DmxAttribute &attribute, std::string &errorMessage) const
    {
        if (attribute.type == "string")
        {
            WriteUInt8(output, kAttributeString);
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
            WriteUInt8(output, kAttributeInt);
            WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            return true;
        }
        if (attribute.type == "float")
        {
            WriteUInt8(output, kAttributeFloat);
            WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            return true;
        }
        if (attribute.type == "bool")
        {
            WriteUInt8(output, kAttributeBool);
            const bool boolValue = attribute.value == "1" || attribute.value == "true";
            WriteUInt8(output, boolValue ? 1 : 0);
            return true;
        }
        if (attribute.type == "vector2" && values.size() >= 2)
        {
            WriteUInt8(output, kAttributeVector2);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            return true;
        }
        if (attribute.type == "vector3" && values.size() >= 3)
        {
            WriteUInt8(output, kAttributeVector3);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            return true;
        }
        if (attribute.type == "vector4" && values.size() >= 4)
        {
            WriteUInt8(output, kAttributeVector4);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }
        if (attribute.type == "quaternion" && values.size() >= 4)
        {
            WriteUInt8(output, kAttributeQuaternion);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }

        errorMessage = "Binary DMX export failed: unsupported scalar attribute type '" + attribute.type + "'.";
        return false;
    }

    bool WriteScalarArray(std::string &output, const DmxAttribute &attribute, std::string &errorMessage) const
    {
        auto writeVectorArray = [&](std::uint8_t type, int components) -> bool
        {
            WriteUInt8(output, type);
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

        if (attribute.type == "int_array")
        {
            WriteUInt8(output, kAttributeIntArray);
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
            WriteUInt8(output, kAttributeFloatArray);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            }
            return true;
        }
        if (attribute.type == "string_array")
        {
            WriteUInt8(output, kAttributeStringArray);
            WriteInt32(output, static_cast<std::int32_t>(attribute.scalarArray.size()));
            for (const std::string &value : attribute.scalarArray)
            {
                WriteCString(output, value);
            }
            return true;
        }
        if (attribute.type == "vector2_array")
        {
            return writeVectorArray(kAttributeVector2Array, 2);
        }
        if (attribute.type == "vector3_array")
        {
            return writeVectorArray(kAttributeVector3Array, 3);
        }
        if (attribute.type == "vector4_array")
        {
            return writeVectorArray(kAttributeVector4Array, 4);
        }
        if (attribute.type == "quaternion_array")
        {
            return writeVectorArray(kAttributeQuaternionArray, 4);
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

DmxAttribute MakeScalarAttribute(const std::string &name, const std::string &type, const std::string &value)
{
    DmxAttribute attribute;
    attribute.kind = DmxAttribute::Kind::Scalar;
    attribute.name = name;
    attribute.type = type;
    attribute.value = value;
    return attribute;
}

DmxAttribute MakeScalarArrayAttribute(const std::string &name, const std::string &type, std::vector<std::string> values)
{
    DmxAttribute attribute;
    attribute.kind = DmxAttribute::Kind::ScalarArray;
    attribute.name = name;
    attribute.type = type;
    attribute.scalarArray = std::move(values);
    return attribute;
}

DmxAttribute MakeInlineElementAttribute(const std::string &name, DmxElement *element)
{
    DmxAttribute attribute;
    attribute.kind = DmxAttribute::Kind::InlineElement;
    attribute.name = name;
    attribute.inlineElement = element;
    return attribute;
}

DmxAttribute MakeElementArrayAttribute(const std::string &name, const std::vector<DmxElement *> &elements)
{
    DmxAttribute attribute;
    attribute.kind = DmxAttribute::Kind::ElementArray;
    attribute.name = name;
    attribute.elementArray = elements;
    return attribute;
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

void AppendSkinningData(const MDagPath &meshPath, DmxElement &vertexDataElement, const ExportContext &context)
{
    MString skinClusterNodeName;
    const MString command = MString("findRelatedSkinCluster \"") + meshPath.fullPathName() + "\"";
    if (MGlobal::executeCommand(command, skinClusterNodeName) != MS::kSuccess || skinClusterNodeName.length() == 0)
    {
        return;
    }

    MSelectionList selectionList;
    if (selectionList.add(skinClusterNodeName) != MS::kSuccess)
    {
        return;
    }

    MObject skinClusterObject;
    if (selectionList.getDependNode(0, skinClusterObject) != MS::kSuccess)
    {
        return;
    }

    MStatus status;
    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return;
    }

    MIntArray vertexIds;
    const unsigned int vertexCount = meshFn.numVertices(&status);
    if (!status || vertexCount == 0)
    {
        return;
    }

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }

    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return;
    }

    MDagPathArray influencePaths;
    const unsigned int influenceCount = skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status || influenceCount == 0)
    {
        return;
    }

    std::vector<int> influenceToJointIndex(influenceCount, -1);
    for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
    {
        const std::string pathKey = DagPathKey(influencePaths[influenceIndex]);
        auto it = context.jointIndexByPath.find(pathKey);
        if (it != context.jointIndexByPath.end())
        {
            influenceToJointIndex[influenceIndex] = it->second;
        }
    }

    MDoubleArray weights;
    unsigned int exportedInfluenceCount = 0;
    status = skinClusterFn.getWeights(meshPath, vertexComponent, weights, exportedInfluenceCount);
    if (!status || exportedInfluenceCount != influenceCount)
    {
        return;
    }

    constexpr double kWeightEpsilon = 1.0e-5;
    unsigned int jointCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        unsigned int activeCount = 0;
        const unsigned int baseOffset = vertexIndex * influenceCount;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            if (influenceToJointIndex[influenceIndex] < 0)
            {
                continue;
            }

            if (std::abs(weights[baseOffset + influenceIndex]) > kWeightEpsilon)
            {
                ++activeCount;
            }
        }

        jointCount = std::max(jointCount, activeCount);
    }

    if (jointCount == 0)
    {
        return;
    }

    std::vector<std::string> jointWeightValues;
    std::vector<std::string> jointIndexValues;
    jointWeightValues.reserve(static_cast<size_t>(vertexCount) * jointCount);
    jointIndexValues.reserve(static_cast<size_t>(vertexCount) * jointCount);

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const unsigned int baseOffset = vertexIndex * influenceCount;
        std::vector<std::pair<int, double>> vertexWeights;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            const int jointIndex = influenceToJointIndex[influenceIndex];
            const double weightValue = weights[baseOffset + influenceIndex];
            if (jointIndex < 0 || std::abs(weightValue) <= kWeightEpsilon)
            {
                continue;
            }

            vertexWeights.push_back({jointIndex, weightValue});
        }

        std::sort(vertexWeights.begin(), vertexWeights.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second != rhs.second)
            {
                return lhs.second > rhs.second;
            }
            return lhs.first < rhs.first;
        });

        if (vertexWeights.size() > jointCount)
        {
            vertexWeights.resize(jointCount);

            double weightSum = 0.0;
            for (const auto &entry : vertexWeights)
            {
                weightSum += entry.second;
            }

            if (weightSum > kWeightEpsilon)
            {
                for (auto &entry : vertexWeights)
                {
                    entry.second /= weightSum;
                }
            }
        }

        while (vertexWeights.size() < jointCount)
        {
            vertexWeights.push_back({0, 0.0});
        }

        for (const auto &entry : vertexWeights)
        {
            jointIndexValues.push_back(std::to_string(entry.first));
            jointWeightValues.push_back(FormatFloat(entry.second));
        }
    }

    vertexDataElement.attributes.push_back(MakeScalarAttribute("jointCount", "int", std::to_string(jointCount)));
    vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("jointIndices", "int_array", std::move(jointIndexValues)));
    vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("jointWeights", "float_array", std::move(jointWeightValues)));

    MFnDependencyNode skinClusterNodeFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaDeformerType", "string", "skinCluster"));
    vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaSkinClusterName", "string", skinClusterNodeFn.name().asChar()));

    MPlug skinningMethodPlug = skinClusterNodeFn.findPlug("skinningMethod", true, &status);
    if (status)
    {
        short skinningMethod = 0;
        if (skinningMethodPlug.getValue(skinningMethod) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaSkinningMethod", "int", std::to_string(static_cast<int>(skinningMethod))));
        }
    }

    MPlug maxInfluencesPlug = skinClusterNodeFn.findPlug("maxInfluences", true, &status);
    if (status)
    {
        int maxInfluences = 0;
        if (maxInfluencesPlug.getValue(maxInfluences) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaMaxInfluences", "int", std::to_string(maxInfluences)));
        }
    }

    MPlug maintainMaxInfluencesPlug = skinClusterNodeFn.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        bool maintainMaxInfluences = false;
        if (maintainMaxInfluencesPlug.getValue(maintainMaxInfluences) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaMaintainMaxInfluences", "bool", maintainMaxInfluences ? "1" : "0"));
        }
    }

    MPlug normalizeWeightsPlug = skinClusterNodeFn.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        short normalizeWeights = 0;
        if (normalizeWeightsPlug.getValue(normalizeWeights) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaNormalizeWeights", "int", std::to_string(static_cast<int>(normalizeWeights))));
        }
    }

    MPlug useComponentsPlug = skinClusterNodeFn.findPlug("useComponents", true, &status);
    if (status)
    {
        bool useComponents = false;
        if (useComponentsPlug.getValue(useComponents) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaUseComponents", "bool", useComponents ? "1" : "0"));
        }
    }

    MPlug geomMatrixPlug = skinClusterNodeFn.findPlug("geomMatrix", true, &status);
    if (status)
    {
        const std::string geomMatrixValue = ReadMatrixPlugValue(geomMatrixPlug);
        if (!geomMatrixValue.empty())
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaGeomMatrix", "string", geomMatrixValue));
        }
    }

    std::vector<std::string> bindPreMatrixValues;
    std::vector<std::string> influencePathValues;
    bindPreMatrixValues.reserve(influenceCount);
    influencePathValues.reserve(influenceCount);
    MPlug bindPreMatrixArrayPlug = skinClusterNodeFn.findPlug("bindPreMatrix", true, &status);
    if (status)
    {
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            influencePathValues.push_back(influencePaths[influenceIndex].fullPathName().asChar());

            MPlug bindPreMatrixPlug = bindPreMatrixArrayPlug.elementByLogicalIndex(influenceIndex, &status);
            if (!status)
            {
                bindPreMatrixValues.push_back(FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()));
                status = MS::kSuccess;
                continue;
            }

            const std::string bindPreMatrixValue = ReadMatrixPlugValue(bindPreMatrixPlug);
            bindPreMatrixValues.push_back(
                bindPreMatrixValue.empty() ?
                FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()) :
                bindPreMatrixValue);
        }
    }

    if (!influencePathValues.empty())
    {
        vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("mayaInfluencePaths", "string_array", std::move(influencePathValues)));
    }
    if (!bindPreMatrixValues.empty())
    {
        vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("mayaBindPreMatrix", "string_array", std::move(bindPreMatrixValues)));
    }
}

DmxElement *BuildMeshElement(DmxTextBuilder &builder, const MDagPath &meshPath, const ExportContext &context)
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

    if (!exportedUvSetNames.empty())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("mayaUvSetNames", "string_array", exportedUvSetNames));
    }

    if (useStoredTangents)
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangents", "vector4_array", storedTangents));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangentsIndices", "int_array", storedTangentIndices));
        if (!storedTangentUvSetName.empty())
        {
            vertexDataElement->attributes.push_back(MakeScalarAttribute("mayaTangentUvSetName", "string", storedTangentUvSetName));
        }
    }
    else if (!tangentChannel.values.empty() && tangentChannel.indices.size() == positionsIndices.size())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangents", "vector4_array", tangentChannel.values));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("tangentsIndices", "int_array", tangentChannel.indices));
        if (uvSetNames.length() > 0)
        {
            vertexDataElement->attributes.push_back(MakeScalarAttribute("mayaTangentUvSetName", "string", uvSetNames[0].asChar()));
        }
    }

    if (context.exportSkin)
    {
        AppendSkinningData(meshPath, *vertexDataElement, context);
    }

    std::vector<DmxElement *> deltaStateElements;
    MObject meshNodeObject = meshPath.node();
    if (context.exportDeltaStates)
    {
        MItDependencyGraph dependencyIt(
            meshNodeObject,
            MFn::kBlendShape,
            MItDependencyGraph::kUpstream,
            MItDependencyGraph::kDepthFirst,
            MItDependencyGraph::kNodeLevel,
            &status);
        if (status)
        {
            for (; !dependencyIt.isDone(); dependencyIt.next())
            {
                MObject blendShapeObject = dependencyIt.currentItem(&status);
                if (!status || blendShapeObject.isNull())
                {
                    continue;
                }

                MFnBlendShapeDeformer blendShapeFn(blendShapeObject, &status);
                if (!status)
                {
                    continue;
                }

            MIntArray weightIndices;
            status = blendShapeFn.weightIndexList(weightIndices);
            if (!status)
            {
                continue;
            }

            MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
            if (!status)
            {
                continue;
            }

            MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
            if (!status)
            {
                continue;
            }

                for (unsigned int weightSlot = 0; weightSlot < weightIndices.length(); ++weightSlot)
                {
                const unsigned int weightIndex = static_cast<unsigned int>(weightIndices[weightSlot]);
                MObjectArray targets;
                status = blendShapeFn.getTargets(meshNodeObject, static_cast<int>(weightIndex), targets);
                if (!status || targets.length() == 0)
                {
                    continue;
                }

                MFnDagNode targetDagNode(targets[0], &status);
                if (!status)
                {
                    continue;
                }

                MDagPath targetPath;
                status = targetDagNode.getPath(targetPath);
                if (!status)
                {
                    continue;
                }

                MFnMesh targetMeshFn(targetPath, &status);
                if (!status)
                {
                    continue;
                }

                MPointArray targetPoints;
                status = targetMeshFn.getPoints(targetPoints, MSpace::kObject);
                if (!status || targetPoints.length() != meshPoints.length())
                {
                    continue;
                }

                std::vector<std::string> deltaPositions;
                std::vector<std::string> deltaPositionIndices;
                deltaPositions.reserve(targetPoints.length());
                deltaPositionIndices.reserve(targetPoints.length());
                for (unsigned int pointIndex = 0; pointIndex < targetPoints.length(); ++pointIndex)
                {
                    const double dx = targetPoints[pointIndex].x - meshPoints[pointIndex].x;
                    const double dy = targetPoints[pointIndex].y - meshPoints[pointIndex].y;
                    const double dz = targetPoints[pointIndex].z - meshPoints[pointIndex].z;
                    if (std::abs(dx) < 1.0e-6 && std::abs(dy) < 1.0e-6 && std::abs(dz) < 1.0e-6)
                    {
                        continue;
                    }

                    deltaPositions.push_back(FormatVector3(dx, dy, dz));
                    deltaPositionIndices.push_back(std::to_string(pointIndex));
                }

                if (deltaPositions.empty())
                {
                    continue;
                }

                std::string deltaName = targetDagNode.name().asChar();
                MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(weightIndex, &status);
                if (status)
                {
                    const MString aliasName = weightPlug.partialName(
                        false, false, false, false, false, true, &status);
                    if (status && aliasName.length() > 0)
                    {
                        deltaName = aliasName.asChar();
                    }
                }

                DmxElement *deltaElement = builder.CreateElement("DmeVertexDeltaData");
                deltaElement->attributes.push_back(MakeScalarAttribute("name", "string", deltaName));
                deltaElement->attributes.push_back(MakeScalarArrayAttribute("vertexFormat", "string_array", {"positions"}));
                deltaElement->attributes.push_back(MakeScalarArrayAttribute("positions", "vector3_array", std::move(deltaPositions)));
                deltaElement->attributes.push_back(MakeScalarArrayAttribute("positionsIndices", "int_array", std::move(deltaPositionIndices)));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaDeformerType", "string", "blendShape"));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeNode", "string", blendShapeNodeFn.name().asChar()));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaWeightIndex", "int", std::to_string(weightIndex)));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaTargetName", "string", targetDagNode.name().asChar()));
                MPlug envelopePlug = blendShapeNodeFn.findPlug("envelope", true, &status);
                if (status)
                {
                    float envelope = 1.0f;
                    if (envelopePlug.getValue(envelope) == MS::kSuccess)
                    {
                        deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeEnvelope", "float", FormatFloat(envelope)));
                    }
                }
                MPlug originPlug = blendShapeNodeFn.findPlug("origin", true, &status);
                if (status)
                {
                    short origin = 0;
                    if (originPlug.getValue(origin) == MS::kSuccess)
                    {
                        deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeOrigin", "int", std::to_string(static_cast<int>(origin))));
                    }
                }
                    deltaStateElements.push_back(deltaElement);
                }
            }
        }
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
            AppendFaceSetElement(builder, setFn.name().asChar(), polygonIndices, polygonFaceIndices, &materialData, faceSetElements);
        }

        for (const SetMembership &membership : deferredWholeObjectSets)
        {
            std::vector<int> polygonIndices = FilterUncoveredPolygons(membership.polygonIndices, coveredPolygons);
            AppendFaceSetElement(builder, membership.name.c_str(), polygonIndices, polygonFaceIndices, &membership.materialData, faceSetElements);
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
        AppendFaceSetElement(builder, "default_faces", uncoveredPolygons, polygonFaceIndices, nullptr, faceSetElements);
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

    const std::string elementType = dagPath.hasFn(MFn::kJoint) ? "DmeJoint" : "DmeDag";
    DmxElement *dagElement = builder.CreateElement(elementType);
    dagElement->attributes.push_back(MakeScalarAttribute("name", "string", dagNode.name().asChar()));

    if (dagPath.hasFn(MFn::kJoint))
    {
        const std::string pathKey = DagPathKey(dagPath);
        if (context.jointIndexByPath.find(pathKey) == context.jointIndexByPath.end())
        {
            context.jointIndexByPath[pathKey] = static_cast<int>(context.jointElements.size());
            context.jointElements.push_back(dagElement);
        }
    }

    if (DmxElement *transformElement = BuildTransformElement(builder, dagPath))
    {
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
        context.materialRoot = exportOptions.materialRoot;
        DmxElement *modelElement = builder.CreateElement("DmeModel");
        modelElement->attributes.push_back(MakeScalarAttribute("name", "string", "maya_export"));
        modelElement->attributes.push_back(MakeScalarAttribute("upAxis", "string", exportOptions.upAxis));
        if (!exportOptions.materialRoot.empty())
        {
            modelElement->attributes.push_back(MakeScalarAttribute("mayaMaterialRoot", "string", exportOptions.materialRoot));
        }

        std::vector<DmxElement *> rootChildren;
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
