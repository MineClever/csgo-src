#include "DmxExportTranslator.h"

#include "../common/MayaDmxCommon.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
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
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MObjectArray.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>

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
};

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
        stream << "\n" << Indent(indentLevel) << "{\n";

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
            WriteElement(stream, *attribute.inlineElement, indentLevel);
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

    std::vector<DmxElement> m_elements;
    int m_nextId = 0;
};

constexpr std::uint8_t kAttributeElement = 1;
constexpr std::uint8_t kAttributeInt = 2;
constexpr std::uint8_t kAttributeFloat = 3;
constexpr std::uint8_t kAttributeBool = 4;
constexpr std::uint8_t kAttributeString = 5;
constexpr std::uint8_t kAttributeVector2 = 9;
constexpr std::uint8_t kAttributeVector3 = 10;
constexpr std::uint8_t kAttributeQuaternion = 13;
constexpr std::uint8_t kAttributeElementArray = 15;
constexpr std::uint8_t kAttributeIntArray = 16;
constexpr std::uint8_t kAttributeFloatArray = 17;
constexpr std::uint8_t kAttributeStringArray = 19;
constexpr std::uint8_t kAttributeVector2Array = 23;
constexpr std::uint8_t kAttributeVector3Array = 24;
constexpr std::uint8_t kAttributeQuaternionArray = 27;
constexpr int kCurrentBinaryEncoding = 5;

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
                if (static_cast<int>(values.size()) < components)
                {
                    errorMessage = "Binary DMX export failed: vector array attribute had too few components.";
                    return false;
                }
                for (int component = 0; component < components; ++component)
                {
                    WriteFloat32(output, static_cast<float>(values[static_cast<size_t>(component)]));
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

std::string FormatQuaternion(double x, double y, double z, double w)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z) + " " + FormatFloat(w);
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
        MSelectionList selectionList;
        if (MGlobal::getActiveSelectionList(selectionList) == MS::kSuccess)
        {
            for (unsigned int i = 0; i < selectionList.length(); ++i)
            {
                MDagPath dagPath;
                if (selectionList.getDagPath(i, dagPath) != MS::kSuccess)
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
    std::vector<std::string> uvs;
    std::vector<std::string> uvIndices;
    std::vector<std::vector<int>> polygonFaceIndices;
    std::unordered_map<std::string, int> normalMap;
    std::unordered_map<std::string, int> uvMap;

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

            float2 uv{};
            if (polygonIt.hasUVs() && polygonIt.getUV(localVertex, uv) == MS::kSuccess)
            {
                const std::string uvKey = FormatVector2(uv[0], uv[1]);
                auto [uvIt, inserted] = uvMap.emplace(uvKey, static_cast<int>(uvs.size()));
                if (inserted)
                {
                    uvs.push_back(uvKey);
                }
                uvIndices.push_back(std::to_string(uvIt->second));
            }
        }

        polygonFaceIndices.push_back(std::move(polygonEntries));
    }

    std::vector<std::string> vertexFormat = {"positions"};
    if (!normals.empty() && normalsIndices.size() == positionsIndices.size())
    {
        vertexFormat.push_back("normals");
    }
    if (!uvs.empty() && uvIndices.size() == positionsIndices.size())
    {
        vertexFormat.push_back("textureCoordinates");
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

    if (!uvs.empty() && uvIndices.size() == positionsIndices.size())
    {
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("textureCoordinates", "vector2_array", uvs));
        vertexDataElement->attributes.push_back(MakeScalarArrayAttribute("textureCoordinatesIndices", "int_array", uvIndices));
    }

    AppendSkinningData(meshPath, *vertexDataElement, context);

    std::vector<DmxElement *> deltaStateElements;
    MObject meshNodeObject = meshPath.node();
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
                deltaStateElements.push_back(deltaElement);
            }
        }
    }

    std::vector<DmxElement *> faceSetElements;
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

            std::vector<std::string> faceValues;
            MObject componentObject = setComponents[setIndex];
            if (!componentObject.isNull() && componentObject.hasFn(MFn::kMeshPolygonComponent))
            {
                MFnSingleIndexedComponent componentFn(componentObject, &status);
                if (!status)
                {
                    continue;
                }

                MIntArray elementIndices;
                status = componentFn.getElements(elementIndices);
                if (!status)
                {
                    continue;
                }

                for (unsigned int i = 0; i < elementIndices.length(); ++i)
                {
                    const int polygonIndex = elementIndices[i];
                    if (polygonIndex < 0 || polygonIndex >= static_cast<int>(polygonFaceIndices.size()))
                    {
                        continue;
                    }

                    for (int faceVertexIndex : polygonFaceIndices[polygonIndex])
                    {
                        faceValues.push_back(std::to_string(faceVertexIndex));
                    }
                    faceValues.push_back("-1");
                }
            }

            if (faceValues.empty())
            {
                continue;
            }

            DmxElement *faceSetElement = builder.CreateElement("DmeFaceSet");
            faceSetElement->attributes.push_back(MakeScalarAttribute("name", "string", setFn.name().asChar()));
            faceSetElement->attributes.push_back(MakeScalarArrayAttribute("faces", "int_array", std::move(faceValues)));
            faceSetElements.push_back(faceSetElement);
        }
    }

    if (faceSetElements.empty())
    {
        std::vector<std::string> faces;
        for (const std::vector<int> &polygonEntries : polygonFaceIndices)
        {
            for (int faceVertexIndex : polygonEntries)
            {
                faces.push_back(std::to_string(faceVertexIndex));
            }
            faces.push_back("-1");
        }

        DmxElement *faceSetElement = builder.CreateElement("DmeFaceSet");
        faceSetElement->attributes.push_back(MakeScalarAttribute("name", "string", "default_faces"));
        faceSetElement->attributes.push_back(MakeScalarArrayAttribute("faces", "int_array", std::move(faces)));
        faceSetElements.push_back(faceSetElement);
    }

    DmxElement *meshElement = builder.CreateElement("DmeMesh");
    meshElement->attributes.push_back(MakeScalarAttribute("name", "string", meshFn.name().asChar()));
    meshElement->attributes.push_back(MakeInlineElementAttribute("bindState", vertexDataElement));
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
    const std::vector<MDagPath> exportRoots = CollectExportRoots(mode);
    if (exportRoots.empty())
    {
        return maya_dmx::ReportError("maya_dmx: nothing to export.");
    }

    DmxTextBuilder builder;
    ExportContext context;
    DmxElement *modelElement = builder.CreateElement("DmeModel");
    modelElement->attributes.push_back(MakeScalarAttribute("name", "string", "maya_export"));
    modelElement->attributes.push_back(MakeScalarAttribute("upAxis", "string", "Y"));

    std::vector<DmxElement *> rootChildren;
    for (const MDagPath &rootPath : exportRoots)
    {
        if (DmxElement *child = BuildDagElement(builder, rootPath, context))
        {
            rootChildren.push_back(child);
        }
    }

    if (!rootChildren.empty())
    {
        modelElement->attributes.push_back(MakeElementArrayAttribute("children", rootChildren));
    }
    if (!context.jointElements.empty())
    {
        modelElement->attributes.push_back(MakeElementArrayAttribute("jointList", context.jointElements));
    }

    const bool binaryExport = IsBinaryExportRequested(fileObject, options);

    std::string serialized;
    std::string serializeError;
    if (binaryExport)
    {
        DmxBinarySerializer binarySerializer;
        if (!binarySerializer.Serialize(*modelElement, serialized, serializeError))
        {
            return maya_dmx::ReportError(serializeError.c_str());
        }
    }
    else
    {
        serialized = builder.Serialize(*modelElement);
    }

    std::ofstream output(fileObject.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to open output file ") + fileObject.rawFullName());
    }

    output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    output.close();

    if (!output)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to write output file ") + fileObject.rawFullName());
    }

    return maya_dmx::ReportInfo(
        MString(binaryExport ? "maya_dmx: exported binary DMX to " : "maya_dmx: exported text DMX to ") + fileObject.rawFullName());
}
