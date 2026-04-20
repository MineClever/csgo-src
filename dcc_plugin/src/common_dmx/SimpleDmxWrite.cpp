#include "SimpleDmxWrite.h"
#include "SimpleDmxTypes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simple_dmx
{
namespace detail
{
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

std::string NormalizeTimeString(const std::string &text)
{
    const std::vector<double> values = ParseNumberList(text);
    const double seconds = values.empty() ? 0.0 : values[0];
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(4);
    stream << seconds;
    return stream.str();
}

bool TryDecodeBinaryText(const std::string &text, std::vector<std::uint8_t> &bytes)
{
    bytes.clear();

    auto hexToInt = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        return -1;
    };

    std::string compact;
    compact.reserve(text.size());
    for (char ch : text)
    {
        if (!std::isspace(static_cast<unsigned char>(ch)))
        {
            compact.push_back(ch);
        }
    }

    if ((compact.size() % 2) != 0)
    {
        return false;
    }

    bytes.reserve(compact.size() / 2);
    for (size_t i = 0; i < compact.size(); i += 2)
    {
        const int hi = hexToInt(compact[i]);
        const int lo = hexToInt(compact[i + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }

    return true;
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

std::vector<std::string> GetOrderedAttributeNames(const Element &element)
{
    std::vector<std::string> names;
    names.reserve(element.attributes.size());

    if (!element.attributeOrder.empty())
    {
        std::unordered_set<std::string> seen;
        seen.reserve(element.attributes.size());
        for (const std::string &attributeName : element.attributeOrder)
        {
            auto it = element.attributes.find(attributeName);
            if (it == element.attributes.end())
            {
                continue;
            }

            if (seen.insert(attributeName).second)
            {
                names.push_back(attributeName);
            }
        }

        for (const auto &entry : element.attributes)
        {
            if (seen.insert(entry.first).second)
            {
                names.push_back(entry.first);
            }
        }

        return names;
    }

    for (const auto &entry : element.attributes)
    {
        names.push_back(entry.first);
    }

    std::sort(names.begin(), names.end());
    return names;
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

    for (const std::string &attributeName : GetOrderedAttributeNames(*element))
    {
        auto it = element->attributes.find(attributeName);
        if (it == element->attributes.end())
        {
            continue;
        }

        const Attribute &attribute = it->second;
        if (attribute.kind == Attribute::Kind::Element)
        {
            CollectReachableElements(document, document.ResolveElement(attribute), ordered, visited);
        }
        else if (attribute.kind == Attribute::Kind::ElementArray)
        {
            auto resolved = document.ResolveElementArray(attribute);
            for (const Element *child : resolved)
            {
                CollectReachableElements(document, child, ordered, visited);
            }
        }
    }
}

// Collect all elements that are referenced as inline children of 'element' or its
// descendants.  These must not be emitted at the top level by SerializeDocumentText
// because they will be emitted inline within their parent's attribute body.
void CollectInlineDescendants(
    const Element *element,
    std::unordered_set<const Element *> &inlineDescendants)
{
    for (const auto &entry : element->attributes)
    {
        const Attribute &attr = entry.second;
        if (attr.kind == Attribute::Kind::Element && attr.elementValue.inlineElement)
        {
            const Element *child = attr.elementValue.inlineElement.get();
            if (inlineDescendants.insert(child).second)
            {
                CollectInlineDescendants(child, inlineDescendants);
            }
        }
        else if (attr.kind == Attribute::Kind::ElementArray)
        {
            for (const ElementLink &link : attr.elementArray)
            {
                if (link.inlineElement)
                {
                    const Element *child = link.inlineElement.get();
                    if (inlineDescendants.insert(child).second)
                    {
                        CollectInlineDescendants(child, inlineDescendants);
                    }
                }
            }
        }
    }
}

void WriteElementBody(const Document &document, std::ostringstream &stream, const Element &element, int indentLevel);

void WriteElementText(
    const Document &document,
    std::ostringstream &stream,
    const Element &element,
    int indentLevel,
    const std::string *typeOverride = nullptr)
{
    stream << Indent(indentLevel);
    WriteQuoted(stream, typeOverride ? *typeOverride : element.type);
    stream << "\n";
    WriteElementBody(document, stream, element, indentLevel);
}

void WriteElementBody(const Document &document, std::ostringstream &stream, const Element &element, int indentLevel)
{
    const std::vector<std::string> orderedAttributeNames = GetOrderedAttributeNames(element);
    stream << Indent(indentLevel) << "{\n";
    stream << Indent(indentLevel + 1);
    WriteQuoted(stream, "id");
    stream << " ";
    WriteQuoted(stream, "elementid");
    stream << " ";
    WriteQuoted(stream, element.id);
    stream << "\n";

    if (!element.name.empty())
    {
        stream << Indent(indentLevel + 1);
        WriteQuoted(stream, "name");
        stream << " ";
        WriteQuoted(stream, "string");
        stream << " ";
        WriteQuoted(stream, element.name);
        stream << "\n";
    }

    for (const std::string &attributeName : orderedAttributeNames)
    {
        auto it = element.attributes.find(attributeName);
        if (it == element.attributes.end())
        {
            continue;
        }

        const Attribute &attribute = it->second;
        if (attributeName == "name")
        {
            continue;
        }

        stream << Indent(indentLevel + 1);
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
            stream << "\n" << Indent(indentLevel + 1) << "[\n";
            for (size_t i = 0; i < attribute.stringArray.size(); ++i)
            {
                stream << Indent(indentLevel + 2);
                WriteQuoted(stream, attribute.stringArray[i]);
                if (i + 1 < attribute.stringArray.size())
                {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << Indent(indentLevel + 1) << "]\n";
            break;

        case Attribute::Kind::Element:
        {
            if (attribute.elementValue.inlineElement)
            {
                WriteQuoted(stream, attribute.declaredType.empty() ? attribute.elementValue.inlineElement->type : attribute.declaredType);
                stream << "\n";
                WriteElementBody(document, stream, *attribute.elementValue.inlineElement, indentLevel + 1);
            }
            else
            {
                WriteQuoted(stream, "element");
                stream << " ";
                const Element *target = document.ResolveElement(attribute);
                WriteQuoted(stream, target ? target->id : attribute.elementValue.referenceId);
                stream << "\n";
            }
            break;
        }

        case Attribute::Kind::ElementArray:
        {
            WriteQuoted(stream, "element_array");
            stream << "\n" << Indent(indentLevel + 1) << "[\n";
            for (size_t i = 0; i < attribute.elementArray.size(); ++i)
            {
                stream << Indent(indentLevel + 2);
                if (attribute.elementArray[i].inlineElement)
                {
                    WriteQuoted(stream, attribute.elementArray[i].inlineElement->type);
                    stream << "\n";
                    WriteElementBody(document, stream, *attribute.elementArray[i].inlineElement, indentLevel + 2);
                }
                else
                {
                    WriteQuoted(stream, "element");
                    stream << " ";
                    WriteQuoted(stream, attribute.elementArray[i].referenceId);
                }
                if (i + 1 < attribute.elementArray.size())
                {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << Indent(indentLevel + 1) << "]\n";
            break;
        }
        }
    }

    stream << Indent(indentLevel) << "}\n";
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

            for (const std::string &attributeName : GetOrderedAttributeNames(*element))
            {
                auto it = element->attributes.find(attributeName);
                if (it == element->attributes.end())
                {
                    continue;
                }

                AddString(attributeName);
                if (it->second.kind == Attribute::Kind::String && it->second.declaredType == "string")
                {
                    AddString(it->second.stringValue);
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
            for (const std::string &attributeName : GetOrderedAttributeNames(*element))
            {
                if (attributeName != "name" && element->attributes.find(attributeName) != element->attributes.end())
                {
                    ++attributeCount;
                }
            }

            WriteInt32(output, attributeCount);
            for (const std::string &attributeName : GetOrderedAttributeNames(*element))
            {
                auto it = element->attributes.find(attributeName);
                if (it == element->attributes.end() || attributeName == "name")
                {
                    continue;
                }

                WriteInt32(output, m_stringToIndex[attributeName]);
                if (!WriteAttribute(document, it->second, output, errorMessage))
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
            std::uint8_t typeCode = 0;
            TryGetBinaryTypeCode(ValueType::Element, typeCode);
            WriteUInt8(output, typeCode);
            return WriteElementRef(document.ResolveElement(attribute), output, errorMessage);
        }

        if (attribute.kind == Attribute::Kind::ElementArray)
        {
            std::uint8_t typeCode = 0;
            TryGetBinaryTypeCode(ValueType::ElementArray, typeCode);
            WriteUInt8(output, typeCode);
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
        const ValueType valueType = ValueTypeFromDeclaredType(attribute.declaredType);
        std::uint8_t typeCode = 0;
        if (valueType == ValueType::String && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, m_stringToIndex.at(attribute.stringValue));
            return true;
        }
        if (valueType == ValueType::Int && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            return true;
        }
        if (valueType == ValueType::Float && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            return true;
        }
        if (valueType == ValueType::Bool && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            const bool boolValue = attribute.stringValue == "1" || attribute.stringValue == "true";
            WriteUInt8(output, boolValue ? 1 : 0);
            return true;
        }
        if (valueType == ValueType::Time && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            const double seconds = values.empty() ? 0.0 : values[0];
            const std::int32_t tenThousandths = static_cast<std::int32_t>(std::floor(seconds * 10000.0 + 0.5));
            WriteInt32(output, tenThousandths);
            return true;
        }
        if (valueType == ValueType::Color && values.size() >= 4 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            for (int i = 0; i < 4; ++i)
            {
                const int component = static_cast<int>(values[static_cast<size_t>(i)]);
                WriteUInt8(output, static_cast<std::uint8_t>(component < 0 ? 0 : (component > 255 ? 255 : component)));
            }
            return true;
        }
        if (valueType == ValueType::Vector2 && values.size() >= 2 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            return true;
        }
        if (valueType == ValueType::Vector3 && values.size() >= 3 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            return true;
        }
        if (valueType == ValueType::Vector4 && values.size() >= 4 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }
        if (valueType == ValueType::QAngle && values.size() >= 3 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            return true;
        }
        if (valueType == ValueType::Quaternion && values.size() >= 4 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteFloat32(output, static_cast<float>(values[0]));
            WriteFloat32(output, static_cast<float>(values[1]));
            WriteFloat32(output, static_cast<float>(values[2]));
            WriteFloat32(output, static_cast<float>(values[3]));
            return true;
        }
        if (valueType == ValueType::VMatrix && values.size() >= 16 && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            for (int i = 0; i < 16; ++i)
            {
                WriteFloat32(output, static_cast<float>(values[static_cast<size_t>(i)]));
            }
            return true;
        }
        if (valueType == ValueType::Void && TryGetBinaryTypeCode(valueType, typeCode))
        {
            std::vector<std::uint8_t> blob;
            if (!TryDecodeBinaryText(attribute.stringValue, blob))
            {
                errorMessage = "Binary DMX export failed: invalid binary text for '" + attribute.declaredType + "'.";
                return false;
            }

            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(blob.size()));
            if (!blob.empty())
            {
                output.append(reinterpret_cast<const char *>(blob.data()), blob.size());
            }
            return true;
        }

        errorMessage = "Binary DMX export failed: unsupported scalar type '" + attribute.declaredType + "'.";
        return false;
    }

    bool WriteArray(const Attribute &attribute, std::string &output, std::string &errorMessage) const
    {
        const auto writeVectorArray = [&](ValueType valueType, int components) -> bool
        {
            std::uint8_t typeCode = 0;
            if (!TryGetBinaryTypeCode(valueType, typeCode))
            {
                return false;
            }

            WriteUInt8(output, typeCode);
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

        const ValueType valueType = ValueTypeFromDeclaredType(attribute.declaredType);
        std::uint8_t typeCode = 0;
        if (valueType == ValueType::IntArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteInt32(output, values.empty() ? 0 : static_cast<std::int32_t>(values[0]));
            }
            return true;
        }
        if (valueType == ValueType::FloatArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                WriteFloat32(output, values.empty() ? 0.0f : static_cast<float>(values[0]));
            }
            return true;
        }
        if (valueType == ValueType::BoolArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const bool boolValue = value == "1" || value == "true";
                WriteUInt8(output, boolValue ? 1 : 0);
            }
            return true;
        }
        if (valueType == ValueType::StringArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                WriteCString(output, value);
            }
            return true;
        }
        if (valueType == ValueType::TimeArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                const double seconds = values.empty() ? 0.0 : values[0];
                const std::int32_t tenThousandths = static_cast<std::int32_t>(std::floor(seconds * 10000.0 + 0.5));
                WriteInt32(output, tenThousandths);
            }
            return true;
        }
        if (valueType == ValueType::ColorArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                if (values.size() < 4)
                {
                    errorMessage = "Binary DMX export failed: unsupported array type '" + attribute.declaredType + "'.";
                    return false;
                }
                for (int i = 0; i < 4; ++i)
                {
                    const int component = static_cast<int>(values[static_cast<size_t>(i)]);
                    WriteUInt8(output, static_cast<std::uint8_t>(component < 0 ? 0 : (component > 255 ? 255 : component)));
                }
            }
            return true;
        }
        if (valueType == ValueType::Vector2Array)
        {
            if (writeVectorArray(valueType, 2))
            {
                return true;
            }
        }
        else if (valueType == ValueType::Vector3Array)
        {
            if (writeVectorArray(valueType, 3))
            {
                return true;
            }
        }
        else if (valueType == ValueType::Vector4Array)
        {
            if (writeVectorArray(valueType, 4))
            {
                return true;
            }
        }
        else if (valueType == ValueType::QAngleArray)
        {
            if (writeVectorArray(valueType, 3))
            {
                return true;
            }
        }
        else if (valueType == ValueType::QuaternionArray)
        {
            if (writeVectorArray(valueType, 4))
            {
                return true;
            }
        }
        else if (valueType == ValueType::VMatrixArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                const std::vector<double> values = ParseNumberList(value);
                if (values.size() < 16)
                {
                    errorMessage = "Binary DMX export failed: unsupported array type '" + attribute.declaredType + "'.";
                    return false;
                }
                for (int i = 0; i < 16; ++i)
                {
                    WriteFloat32(output, static_cast<float>(values[static_cast<size_t>(i)]));
                }
            }
            return true;
        }
        else if (valueType == ValueType::VoidArray && TryGetBinaryTypeCode(valueType, typeCode))
        {
            WriteUInt8(output, typeCode);
            WriteInt32(output, static_cast<std::int32_t>(attribute.stringArray.size()));
            for (const std::string &value : attribute.stringArray)
            {
                std::vector<std::uint8_t> blob;
                if (!TryDecodeBinaryText(value, blob))
                {
                    errorMessage = "Binary DMX export failed: invalid binary text for '" + attribute.declaredType + "'.";
                    return false;
                }
                WriteInt32(output, static_cast<std::int32_t>(blob.size()));
                if (!blob.empty())
                {
                    output.append(reinterpret_cast<const char *>(blob.data()), blob.size());
                }
            }
            return true;
        }

        errorMessage = "Binary DMX export failed: unsupported array type '" + attribute.declaredType + "'.";
        return false;
    }

    std::vector<const Element *> m_elements;
    std::unordered_map<const Element *, std::int32_t> m_elementToIndex;
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, std::int32_t> m_stringToIndex;
};
} // namespace detail

using namespace detail;

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

    // Elements that appear as inline children must not be emitted at the top level;
    // they are emitted recursively inside their parent's attribute body.
    std::unordered_set<const Element *> inlineDescendants;
    for (const Element *element : ordered)
    {
        CollectInlineDescendants(element, inlineDescendants);
    }

    for (const Element *element : ordered)
    {
        if (inlineDescendants.find(element) == inlineDescendants.end())
        {
            WriteElementText(document, stream, *element, 0);
        }
    }

    return stream.str();
}

bool SerializeDocumentBinary(const Document &document, std::string &output, std::string &errorMessage)
{
    BinarySerializer serializer;
    return serializer.Serialize(document, output, errorMessage);
}
}
