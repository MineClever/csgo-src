#include "SimpleDmxBinary.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <sstream>
#include <unordered_set>

namespace simple_dmx
{
namespace
{
constexpr int kAttributeElement = 1;
constexpr int kAttributeInt = 2;
constexpr int kAttributeFloat = 3;
constexpr int kAttributeBool = 4;
constexpr int kAttributeString = 5;
constexpr int kAttributeVector2 = 9;
constexpr int kAttributeVector3 = 10;
constexpr int kAttributeQuaternion = 13;
constexpr int kAttributeElementArray = 15;
constexpr int kAttributeIntArray = 16;
constexpr int kAttributeFloatArray = 17;
constexpr int kAttributeStringArray = 19;
constexpr int kAttributeVector2Array = 23;
constexpr int kAttributeVector3Array = 24;
constexpr int kAttributeQuaternionArray = 27;
constexpr int kCurrentBinaryEncoding = 5;

std::string MakeError(const char *message)
{
    return std::string("Binary DMX parse failed: ") + message;
}

std::string IdBytesToString(const std::array<std::uint8_t, 16> &bytes)
{
    char buffer[33] = {};
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        std::snprintf(buffer + i * 2, 3, "%02x", bytes[i]);
    }
    return buffer;
}

class BinaryReader
{
public:
    explicit BinaryReader(const std::string &bytes)
        : m_bytes(bytes)
    {
    }

    bool ReadHeader(bool &isBinary, int &encodingVersion, std::string &errorMessage)
    {
        if (m_bytes.rfind("<!-- dmx ", 0) != 0)
        {
            errorMessage = "Missing DMX header.";
            return false;
        }

        const size_t headerEnd = m_bytes.find("-->");
        if (headerEnd == std::string::npos)
        {
            errorMessage = "Incomplete DMX header.";
            return false;
        }

        std::string header = m_bytes.substr(0, headerEnd + 3);
        char encodingName[32] = {};
        char formatName[32] = {};
        int formatVersion = 0;
        const int assigned = std::sscanf(
            header.c_str(),
            "<!-- dmx encoding %31s %d format %31s %d -->",
            encodingName,
            &encodingVersion,
            formatName,
            &formatVersion);
        if (assigned != 4)
        {
            errorMessage = "Unrecognized DMX header.";
            return false;
        }

        isBinary = std::string(encodingName) == "binary";
        m_position = headerEnd + 3;

        if (isBinary)
        {
            while (m_position < m_bytes.size() && m_bytes[m_position] != '\0')
            {
                ++m_position;
            }
            if (m_position >= m_bytes.size())
            {
                errorMessage = "Binary DMX header terminator was missing.";
                return false;
            }
            ++m_position;
        }

        return true;
    }

    bool ReadInt32(std::int32_t &value)
    {
        return ReadPod(value);
    }

    bool ReadUInt8(std::uint8_t &value)
    {
        return ReadPod(value);
    }

    bool ReadFloat(float &value)
    {
        return ReadPod(value);
    }

    bool ReadBytes(void *destination, size_t byteCount)
    {
        if (m_position + byteCount > m_bytes.size())
        {
            return false;
        }

        std::memcpy(destination, m_bytes.data() + m_position, byteCount);
        m_position += byteCount;
        return true;
    }

    bool ReadCString(std::string &value)
    {
        const size_t end = m_bytes.find('\0', m_position);
        if (end == std::string::npos)
        {
            return false;
        }

        value.assign(m_bytes.data() + m_position, end - m_position);
        m_position = end + 1;
        return true;
    }

    size_t Position() const
    {
        return m_position;
    }

private:
    template <typename T>
    bool ReadPod(T &value)
    {
        return ReadBytes(&value, sizeof(T));
    }

    const std::string &m_bytes;
    size_t m_position = 0;
};

std::string FormatFloat(float value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str();
}

std::string FormatVector(const std::vector<float> &values)
{
    std::ostringstream stream;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            stream << ' ';
        }
        stream << FormatFloat(values[i]);
    }
    return stream.str();
}

bool ReadStringTable(BinaryReader &reader, std::vector<std::string> &strings, std::string &errorMessage)
{
    std::int32_t stringCount = 0;
    if (!reader.ReadInt32(stringCount) || stringCount < 0)
    {
        errorMessage = MakeError("invalid string table count");
        return false;
    }

    strings.clear();
    strings.reserve(static_cast<size_t>(stringCount));
    for (std::int32_t i = 0; i < stringCount; ++i)
    {
        std::string value;
        if (!reader.ReadCString(value))
        {
            errorMessage = MakeError("unexpected end of string table");
            return false;
        }
        strings.push_back(std::move(value));
    }

    return true;
}

const std::string *LookupString(const std::vector<std::string> &strings, std::int32_t index)
{
    if (index < 0 || index >= static_cast<std::int32_t>(strings.size()))
    {
        return nullptr;
    }
    return &strings[static_cast<size_t>(index)];
}

bool ReadElementReference(
    BinaryReader &reader,
    const std::vector<std::shared_ptr<Element>> &elements,
    ElementLink &link,
    std::string &errorMessage)
{
    std::int32_t elementIndex = -1;
    if (!reader.ReadInt32(elementIndex))
    {
        errorMessage = MakeError("unexpected end while reading element reference");
        return false;
    }

    if (elementIndex < 0)
    {
        link.referenceId.clear();
        return true;
    }

    if (elementIndex >= static_cast<std::int32_t>(elements.size()) || !elements[static_cast<size_t>(elementIndex)])
    {
        errorMessage = MakeError("element reference was out of range");
        return false;
    }

    link.referenceId = elements[static_cast<size_t>(elementIndex)]->id;
    return true;
}

bool ReadScalarAttribute(
    BinaryReader &reader,
    int attributeType,
    Attribute &attribute,
    std::string &errorMessage)
{
    attribute.kind = Attribute::Kind::String;

    switch (attributeType)
    {
    case kAttributeInt:
    {
        std::int32_t value = 0;
        if (!reader.ReadInt32(value))
        {
            errorMessage = MakeError("unexpected end while reading int");
            return false;
        }
        attribute.stringValue = std::to_string(value);
        return true;
    }

    case kAttributeFloat:
    {
        float value = 0.0f;
        if (!reader.ReadFloat(value))
        {
            errorMessage = MakeError("unexpected end while reading float");
            return false;
        }
        attribute.stringValue = FormatFloat(value);
        return true;
    }

    case kAttributeBool:
    {
        std::uint8_t value = 0;
        if (!reader.ReadUInt8(value))
        {
            errorMessage = MakeError("unexpected end while reading bool");
            return false;
        }
        attribute.stringValue = value != 0 ? "1" : "0";
        return true;
    }

    case kAttributeVector2:
    case kAttributeVector3:
    case kAttributeQuaternion:
    {
        const int componentCount = attributeType == kAttributeVector2 ? 2 : (attributeType == kAttributeVector3 ? 3 : 4);
        std::vector<float> values(static_cast<size_t>(componentCount), 0.0f);
        for (int i = 0; i < componentCount; ++i)
        {
            if (!reader.ReadFloat(values[static_cast<size_t>(i)]))
            {
                errorMessage = MakeError("unexpected end while reading vector value");
                return false;
            }
        }
        attribute.stringValue = FormatVector(values);
        return true;
    }

    default:
        errorMessage = MakeError("unsupported scalar attribute type");
        return false;
    }
}

bool ReadArrayAttribute(
    BinaryReader &reader,
    int attributeType,
    Attribute &attribute,
    std::string &errorMessage)
{
    attribute.kind = Attribute::Kind::StringArray;

    std::int32_t count = 0;
    if (!reader.ReadInt32(count) || count < 0)
    {
        errorMessage = MakeError("invalid array count");
        return false;
    }

    attribute.stringArray.clear();
    attribute.stringArray.reserve(static_cast<size_t>(count));

    auto readVectorArray = [&](int componentCount) -> bool
    {
        std::vector<float> values(static_cast<size_t>(componentCount), 0.0f);
        for (std::int32_t i = 0; i < count; ++i)
        {
            for (int component = 0; component < componentCount; ++component)
            {
                if (!reader.ReadFloat(values[static_cast<size_t>(component)]))
                {
                    errorMessage = MakeError("unexpected end while reading vector array");
                    return false;
                }
            }
            attribute.stringArray.push_back(FormatVector(values));
        }
        return true;
    };

    switch (attributeType)
    {
    case kAttributeIntArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::int32_t value = 0;
            if (!reader.ReadInt32(value))
            {
                errorMessage = MakeError("unexpected end while reading int array");
                return false;
            }
            attribute.stringArray.push_back(std::to_string(value));
        }
        return true;

    case kAttributeFloatArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            float value = 0.0f;
            if (!reader.ReadFloat(value))
            {
                errorMessage = MakeError("unexpected end while reading float array");
                return false;
            }
            attribute.stringArray.push_back(FormatFloat(value));
        }
        return true;

    case kAttributeStringArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::string value;
            if (!reader.ReadCString(value))
            {
                errorMessage = MakeError("unexpected end while reading string array");
                return false;
            }
            attribute.stringArray.push_back(std::move(value));
        }
        return true;

    case kAttributeVector2Array:
        return readVectorArray(2);

    case kAttributeVector3Array:
        return readVectorArray(3);

    case kAttributeQuaternionArray:
        return readVectorArray(4);

    default:
        errorMessage = MakeError("unsupported array attribute type");
        return false;
    }
}
}

bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage)
{
    document.m_root.reset();
    document.m_ownedElements.clear();
    document.m_elementsById.clear();

    BinaryReader reader(bytes);
    bool isBinary = false;
    int encodingVersion = 0;
    if (!reader.ReadHeader(isBinary, encodingVersion, errorMessage))
    {
        return false;
    }

    if (!isBinary)
    {
        errorMessage = "DMX header did not declare binary encoding.";
        return false;
    }

    if (encodingVersion < 0 || encodingVersion > kCurrentBinaryEncoding)
    {
        errorMessage = MakeError("unsupported binary encoding version");
        return false;
    }

    std::vector<std::string> strings;
    if (!ReadStringTable(reader, strings, errorMessage))
    {
        return false;
    }

    std::int32_t elementCount = 0;
    if (!reader.ReadInt32(elementCount) || elementCount < 0)
    {
        errorMessage = MakeError("invalid element count");
        return false;
    }

    std::vector<std::shared_ptr<Element>> elements(static_cast<size_t>(elementCount));
    for (std::int32_t i = 0; i < elementCount; ++i)
    {
        std::int32_t typeIndex = -1;
        std::int32_t nameIndex = -1;
        std::array<std::uint8_t, 16> idBytes{};
        if (!reader.ReadInt32(typeIndex) || !reader.ReadInt32(nameIndex) || !reader.ReadBytes(idBytes.data(), idBytes.size()))
        {
            errorMessage = MakeError("unexpected end while reading element dictionary");
            return false;
        }

        const std::string *typeName = LookupString(strings, typeIndex);
        const std::string *nameValue = LookupString(strings, nameIndex);
        if (!typeName || !nameValue)
        {
            errorMessage = MakeError("element dictionary referenced an invalid string");
            return false;
        }

        auto element = std::make_shared<Element>();
        element->type = *typeName;
        element->name = *nameValue;
        element->id = IdBytesToString(idBytes);

        Attribute nameAttribute;
        nameAttribute.kind = Attribute::Kind::String;
        nameAttribute.declaredType = "string";
        nameAttribute.stringValue = element->name;
        element->attributes.emplace("name", std::move(nameAttribute));

        document.m_ownedElements.push_back(element);
        document.m_elementsById[element->id] = element.get();
        elements[static_cast<size_t>(i)] = element;
    }

    if (!elements.empty())
    {
        document.m_root = elements.front();
    }

    for (std::int32_t elementIndex = 0; elementIndex < elementCount; ++elementIndex)
    {
        std::shared_ptr<Element> &element = elements[static_cast<size_t>(elementIndex)];

        std::int32_t attributeCount = 0;
        if (!reader.ReadInt32(attributeCount) || attributeCount < 0)
        {
            errorMessage = MakeError("invalid attribute count");
            return false;
        }

        for (std::int32_t attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex)
        {
            std::int32_t nameIndex = -1;
            std::uint8_t attributeType = 0;
            if (!reader.ReadInt32(nameIndex) || !reader.ReadUInt8(attributeType))
            {
                errorMessage = MakeError("unexpected end while reading attribute header");
                return false;
            }

            const std::string *attributeName = LookupString(strings, nameIndex);
            if (!attributeName)
            {
                errorMessage = MakeError("attribute name referenced an invalid string");
                return false;
            }

            Attribute attribute;
            switch (attributeType)
            {
            case kAttributeElement:
                attribute.kind = Attribute::Kind::Element;
                attribute.declaredType = "element";
                if (!ReadElementReference(reader, elements, attribute.elementValue, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeElementArray:
            {
                attribute.kind = Attribute::Kind::ElementArray;
                attribute.declaredType = "element_array";

                std::int32_t count = 0;
                if (!reader.ReadInt32(count) || count < 0)
                {
                    errorMessage = MakeError("invalid element array count");
                    return false;
                }

                attribute.elementArray.reserve(static_cast<size_t>(count));
                for (std::int32_t i = 0; i < count; ++i)
                {
                    ElementLink link;
                    if (!ReadElementReference(reader, elements, link, errorMessage))
                    {
                        return false;
                    }
                    attribute.elementArray.push_back(std::move(link));
                }
                break;
            }

            case kAttributeString:
            {
                attribute.kind = Attribute::Kind::String;
                attribute.declaredType = "string";
                std::int32_t valueIndex = -1;
                if (!reader.ReadInt32(valueIndex))
                {
                    errorMessage = MakeError("unexpected end while reading string value");
                    return false;
                }
                const std::string *value = LookupString(strings, valueIndex);
                if (!value)
                {
                    errorMessage = MakeError("string value referenced an invalid string");
                    return false;
                }
                attribute.stringValue = *value;
                break;
            }

            case kAttributeInt:
                attribute.declaredType = "int";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeFloat:
                attribute.declaredType = "float";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeBool:
                attribute.declaredType = "bool";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeVector2:
                attribute.declaredType = "vector2";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeVector3:
                attribute.declaredType = "vector3";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeQuaternion:
                attribute.declaredType = "quaternion";
                if (!ReadScalarAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeIntArray:
                attribute.declaredType = "int_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeFloatArray:
                attribute.declaredType = "float_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeStringArray:
                attribute.declaredType = "string_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeVector2Array:
                attribute.declaredType = "vector2_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeVector3Array:
                attribute.declaredType = "vector3_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case kAttributeQuaternionArray:
                attribute.declaredType = "quaternion_array";
                if (!ReadArrayAttribute(reader, attributeType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            default:
                errorMessage = MakeError("encountered an unsupported attribute type in binary DMX");
                return false;
            }

            if (*attributeName == "name" && attribute.kind == Attribute::Kind::String)
            {
                element->name = attribute.stringValue;
            }

            element->attributes[*attributeName] = std::move(attribute);
        }
    }

    return true;
}
}
