#include "SimpleDmxWrite.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simple_dmx
{
namespace
{
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

void WriteQuoted(std::ostringstream &stream, const std::string &value)
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

std::string Indent(int level)
{
    return std::string(level * 4, ' ');
}

void CollectReachableElements(
    const Document &document,
    const Element *element,
    std::vector<const Element *> &ordered,
    std::unordered_set<const Element *> &visited)
{
    if (!element || visited.find(element) != visited.end())
    {
        return;
    }

    visited.insert(element);
    ordered.push_back(element);

    for (const auto &entry : element->attributes)
    {
        const Attribute &attribute = entry.second;
        if (attribute.kind == Attribute::Kind::Element)
        {
            CollectReachableElements(document, document.ResolveElement(attribute), ordered, visited);
        }
        else if (attribute.kind == Attribute::Kind::ElementArray)
        {
            for (const Element *child : document.ResolveElementArray(attribute))
            {
                CollectReachableElements(document, child, ordered, visited);
            }
        }
    }
}

void WriteElementText(const Document &document, std::ostringstream &stream, const Element &element)
{
    WriteQuoted(stream, element.type);
    stream << "\n{\n";
    stream << Indent(1);
    WriteQuoted(stream, "id");
    stream << " ";
    WriteQuoted(stream, "elementid");
    stream << " ";
    WriteQuoted(stream, element.id);
    stream << "\n";

    if (!element.name.empty())
    {
        stream << Indent(1);
        WriteQuoted(stream, "name");
        stream << " ";
        WriteQuoted(stream, "string");
        stream << " ";
        WriteQuoted(stream, element.name);
        stream << "\n";
    }

    for (const auto &entry : element.attributes)
    {
        const std::string &attributeName = entry.first;
        const Attribute &attribute = entry.second;
        if (attributeName == "name")
        {
            continue;
        }

        stream << Indent(1);
        WriteQuoted(stream, attributeName);
        stream << " ";

        switch (attribute.kind)
        {
        case Attribute::Kind::String:
            WriteQuoted(stream, attribute.declaredType);
            stream << " ";
            WriteQuoted(stream, attribute.stringValue);
            stream << "\n";
            break;

        case Attribute::Kind::StringArray:
            WriteQuoted(stream, attribute.declaredType);
            stream << "\n" << Indent(1) << "[\n";
            for (size_t i = 0; i < attribute.stringArray.size(); ++i)
            {
                stream << Indent(2);
                WriteQuoted(stream, attribute.stringArray[i]);
                if (i + 1 < attribute.stringArray.size())
                {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << Indent(1) << "]\n";
            break;

        case Attribute::Kind::Element:
        {
            WriteQuoted(stream, "element");
            stream << " ";
            const Element *target = document.ResolveElement(attribute);
            WriteQuoted(stream, target ? target->id : attribute.elementValue.referenceId);
            stream << "\n";
            break;
        }

        case Attribute::Kind::ElementArray:
        {
            WriteQuoted(stream, "element_array");
            stream << "\n" << Indent(1) << "[\n";
            const std::vector<const Element *> targets = document.ResolveElementArray(attribute);
            for (size_t i = 0; i < attribute.elementArray.size(); ++i)
            {
                stream << Indent(2);
                WriteQuoted(stream, "element");
                stream << " ";
                const Element *target = i < targets.size() ? targets[i] : nullptr;
                const std::string referenceId = target ? target->id : attribute.elementArray[i].referenceId;
                WriteQuoted(stream, referenceId);
                if (i + 1 < attribute.elementArray.size())
                {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << Indent(1) << "]\n";
            break;
        }
        }
    }

    stream << "}\n";
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

class BinarySerializer
{
public:
    bool Serialize(const Document &document, std::string &output, std::string &errorMessage)
    {
        const Element *root = document.GetRoot();
        if (!root)
        {
            errorMessage = "Binary DMX export failed: document had no root element.";
            return false;
        }

        std::vector<const Element *> elements;
        std::unordered_set<const Element *> visited;
        CollectReachableElements(document, root, elements, visited);

        for (const Element *element : elements)
        {
            m_elementToIndex[element] = static_cast<std::int32_t>(m_elements.size());
            m_elements.push_back(element);
            AddString(element->type);
            AddString(element->name);

            for (const auto &entry : element->attributes)
            {
                AddString(entry.first);
                if (entry.second.kind == Attribute::Kind::String && entry.second.declaredType == "string")
                {
                    AddString(entry.second.stringValue);
                }
            }
        }

        output.clear();
        output += "<!-- dmx encoding binary 5 format dmx 1 -->\n";
        output.push_back('\0');

        WriteInt32(output, static_cast<std::int32_t>(m_strings.size()));
        for (const std::string &value : m_strings)
        {
            WriteCString(output, value);
        }

        WriteInt32(output, static_cast<std::int32_t>(m_elements.size()));
        for (const Element *element : m_elements)
        {
            WriteInt32(output, m_stringToIndex[element->type]);
            WriteInt32(output, m_stringToIndex[element->name]);
            const auto idBytes = MakeElementIdBytes(element->id);
            output.append(reinterpret_cast<const char *>(idBytes.data()), idBytes.size());
        }

        for (const Element *element : m_elements)
        {
            int attributeCount = 0;
            for (const auto &entry : element->attributes)
            {
                if (entry.first != "name")
                {
                    ++attributeCount;
                }
            }

            WriteInt32(output, attributeCount);
            for (const auto &entry : element->attributes)
            {
                if (entry.first == "name")
                {
                    continue;
                }

                WriteInt32(output, m_stringToIndex[entry.first]);
                if (!WriteAttribute(document, entry.second, output, errorMessage))
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
        m_stringToIndex[value] = static_cast<std::int32_t>(m_strings.size());
        m_strings.push_back(value);
    }

    bool WriteElementRef(const Element *target, std::string &output, std::string &errorMessage) const
    {
        if (!target)
        {
            WriteInt32(output, -1);
            return true;
        }
        auto it = m_elementToIndex.find(target);
        if (it == m_elementToIndex.end())
        {
            errorMessage = "Binary DMX export failed: unresolved element reference.";
            return false;
        }
        WriteInt32(output, it->second);
        return true;
    }

    bool WriteAttribute(const Document &document, const Attribute &attribute, std::string &output, std::string &errorMessage) const
    {
        if (attribute.kind == Attribute::Kind::Element)
        {
            WriteUInt8(output, kAttributeElement);
            return WriteElementRef(document.ResolveElement(attribute), output, errorMessage);
        }

        if (attribute.kind == Attribute::Kind::ElementArray)
        {
            WriteUInt8(output, kAttributeElementArray);
            const std::vector<const Element *> targets = document.ResolveElementArray(attribute);
            WriteInt32(output, static_cast<std::int32_t>(attribute.elementArray.size()));
            for (size_t i = 0; i < attribute.elementArray.size(); ++i)
            {
                const Element *target = i < targets.size() ? targets[i] : nullptr;
                if (!WriteElementRef(target, output, errorMessage))
                {
                    return false;
                }
            }
            return true;
        }

        if (attribute.kind == Attribute::Kind::String)
        {
            return WriteScalar(attribute, output, errorMessage);
        }

        if (attribute.kind == Attribute::Kind::StringArray)
        {
            return WriteArray(attribute, output, errorMessage);
        }

        errorMessage = "Binary DMX export failed: unsupported attribute kind.";
        return false;
    }

    bool WriteScalar(const Attribute &attribute, std::string &output, std::string &errorMessage) const
    {
        const std::vector<double> values = ParseNumberList(attribute.stringValue);
        if (attribute.declaredType == "string")
        {
            WriteUInt8(output, kAttributeString);
            WriteInt32(output, m_stringToIndex.at(attribute.stringValue));
            return true;
        }
        if (attribute.declaredType == "int")
        {
            WriteUInt8(output, kAttributeInt);
            WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            return true;
        }
        if (attribute.declaredType == "float")
        {
            WriteUInt8(output, kAttributeFloat);
            WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            return true;
        }
        if (attribute.declaredType == "bool")
        {
            WriteUInt8(output, kAttributeBool);
            const bool boolValue = attribute.stringValue == "1" || attribute.stringValue == "true";
            WriteUInt8(output, boolValue ? 1 : 0);
            return true;
        }
        if (attribute.declaredType == "vector2" && values.size() >= 2)
        {
            WriteUInt8(output, kAttributeVector2);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            return true;
        }
        if (attribute.declaredType == "vector3" && values.size() >= 3)
        {
            WriteUInt8(output, kAttributeVector3);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            return true;
        }
        if (attribute.declaredType == "quaternion" && values.size() >= 4)
        {
            WriteUInt8(output, kAttributeQuaternion);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }

        errorMessage = "Binary DMX export failed: unsupported scalar type '" + attribute.declaredType + "'.";
        return false;
    }

    bool WriteArray(const Attribute &attribute, std::string &output, std::string &errorMessage) const
    {
        const auto writeVectorArray = [&](std::uint8_t type, int components) -> bool
        {
            WriteUInt8(output, type);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                if (static_cast<int>(values.size()) < components)
                {
                    return false;
                }
                for (int i = 0; i < components; ++i)
                {
                    WriteFloat32(output, static_cast<float>(values[static_cast<size_t>(i)]));
                }
            }
            return true;
        };

        if (attribute.declaredType == "int_array")
        {
            WriteUInt8(output, kAttributeIntArray);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            }
            return true;
        }
        if (attribute.declaredType == "float_array")
        {
            WriteUInt8(output, kAttributeFloatArray);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            }
            return true;
        }
        if (attribute.declaredType == "string_array")
        {
            WriteUInt8(output, kAttributeStringArray);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                WriteCString(output, value);
            }
            return true;
        }
        if (attribute.declaredType == "vector2_array")
        {
            if (writeVectorArray(kAttributeVector2Array, 2))
            {
                return true;
            }
        }
        else if (attribute.declaredType == "vector3_array")
        {
            if (writeVectorArray(kAttributeVector3Array, 3))
            {
                return true;
            }
        }
        else if (attribute.declaredType == "quaternion_array")
        {
            if (writeVectorArray(kAttributeQuaternionArray, 4))
            {
                return true;
            }
        }

        errorMessage = "Binary DMX export failed: unsupported array type '" + attribute.declaredType + "'.";
        return false;
    }

    std::vector<const Element *> m_elements;
    std::unordered_map<const Element *, std::int32_t> m_elementToIndex;
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, std::int32_t> m_stringToIndex;
};
}

std::string SerializeDocumentText(const Document &document)
{
    std::ostringstream stream;
    stream << "<!-- dmx encoding keyvalues2 1 format model 1 -->\n";

    const Element *root = document.GetRoot();
    if (!root)
    {
        return stream.str();
    }

    std::vector<const Element *> ordered;
    std::unordered_set<const Element *> visited;
    CollectReachableElements(document, root, ordered, visited);
    for (const Element *element : ordered)
    {
        WriteElementText(document, stream, *element);
    }

    return stream.str();
}

bool SerializeDocumentBinary(const Document &document, std::string &output, std::string &errorMessage)
{
    BinarySerializer serializer;
    return serializer.Serialize(document, output, errorMessage);
}
}
