#include "SimpleDmxBinary.h"
#include "SimpleDmxTypes.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <memory>
#include <sstream>

namespace simple_dmx
{
namespace detail
{
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

    bool ReadInt16(std::int16_t &value)
    {
        return ReadPod(value);
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

std::string FormatInt(int value)
{
    return std::to_string(value);
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

std::string FormatTime(std::int32_t tenThousandthsOfSecond)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(4);
    stream << (static_cast<double>(tenThousandthsOfSecond) / 10000.0);
    return stream.str();
}

std::string FormatMatrixText(const std::array<float, 16> &values)
{
    std::ostringstream stream;
    for (int row = 0; row < 4; ++row)
    {
        if (row > 0)
        {
            stream << '\n';
        }

        for (int column = 0; column < 4; ++column)
        {
            if (column > 0)
            {
                stream << ' ';
            }
            stream << FormatFloat(values[static_cast<size_t>(row * 4 + column)]);
        }
    }
    return stream.str();
}

std::string FormatBinaryText(const std::vector<std::uint8_t> &bytes)
{
    std::ostringstream stream;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if ((i % 40) == 0 && i != 0)
        {
            stream << '\n';
        }

        const std::uint8_t value = bytes[i];
        const char upper = (value >> 4) <= 9 ? static_cast<char>('0' + (value >> 4)) : static_cast<char>('A' + ((value >> 4) - 10));
        const char lower = (value & 0xF) <= 9 ? static_cast<char>('0' + (value & 0xF)) : static_cast<char>('A' + ((value & 0xF) - 10));
        stream << upper << lower;
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
    ValueType valueType,
    Attribute &attribute,
    std::string &errorMessage)
{
    attribute.kind = Attribute::Kind::String;

    switch (valueType)
    {
    case ValueType::Int:
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

    case ValueType::Float:
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

    case ValueType::Bool:
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

    case ValueType::Time:
    {
        std::int32_t value = 0;
        if (!reader.ReadInt32(value))
        {
            errorMessage = MakeError("unexpected end while reading time");
            return false;
        }
        attribute.stringValue = FormatTime(value);
        return true;
    }

    case ValueType::Color:
    {
        std::array<std::uint8_t, 4> rgba{};
        if (!reader.ReadBytes(rgba.data(), rgba.size()))
        {
            errorMessage = MakeError("unexpected end while reading color");
            return false;
        }
        attribute.stringValue = FormatInt(rgba[0]) + " " + FormatInt(rgba[1]) + " " + FormatInt(rgba[2]) + " " + FormatInt(rgba[3]);
        return true;
    }

    case ValueType::Vector2:
    case ValueType::Vector3:
    case ValueType::Vector4:
    case ValueType::QAngle:
    case ValueType::Quaternion:
    {
        const int componentCount = ComponentCountForValueType(valueType);
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

    case ValueType::VMatrix:
    {
        std::array<float, 16> matrixValues{};
        if (!reader.ReadBytes(matrixValues.data(), sizeof(float) * matrixValues.size()))
        {
            errorMessage = MakeError("unexpected end while reading matrix");
            return false;
        }
        attribute.stringValue = FormatMatrixText(matrixValues);
        return true;
    }

    case ValueType::Void:
    {
        std::int32_t byteCount = 0;
        if (!reader.ReadInt32(byteCount) || byteCount < 0)
        {
            errorMessage = MakeError("invalid binary blob length");
            return false;
        }

        std::vector<std::uint8_t> blob(static_cast<size_t>(byteCount), 0);
        if (byteCount > 0 && !reader.ReadBytes(blob.data(), blob.size()))
        {
            errorMessage = MakeError("unexpected end while reading binary blob");
            return false;
        }

        attribute.stringValue = FormatBinaryText(blob);
        return true;
    }

    default:
        errorMessage = MakeError("unsupported scalar attribute type");
        return false;
    }
}

bool ReadArrayAttribute(
    BinaryReader &reader,
    ValueType valueType,
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

    switch (valueType)
    {
    case ValueType::IntArray:
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

    case ValueType::FloatArray:
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

    case ValueType::BoolArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::uint8_t value = 0;
            if (!reader.ReadUInt8(value))
            {
                errorMessage = MakeError("unexpected end while reading bool array");
                return false;
            }
            attribute.stringArray.push_back(value != 0 ? "1" : "0");
        }
        return true;

    case ValueType::StringArray:
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

    case ValueType::TimeArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::int32_t value = 0;
            if (!reader.ReadInt32(value))
            {
                errorMessage = MakeError("unexpected end while reading time array");
                return false;
            }
            attribute.stringArray.push_back(FormatTime(value));
        }
        return true;

    case ValueType::ColorArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::array<std::uint8_t, 4> rgba{};
            if (!reader.ReadBytes(rgba.data(), rgba.size()))
            {
                errorMessage = MakeError("unexpected end while reading color array");
                return false;
            }
            attribute.stringArray.push_back(
                FormatInt(rgba[0]) + " " + FormatInt(rgba[1]) + " " + FormatInt(rgba[2]) + " " + FormatInt(rgba[3]));
        }
        return true;

    case ValueType::Vector2Array:
        return readVectorArray(2);

    case ValueType::Vector3Array:
        return readVectorArray(3);

    case ValueType::Vector4Array:
        return readVectorArray(4);

    case ValueType::QAngleArray:
        return readVectorArray(3);

    case ValueType::QuaternionArray:
        return readVectorArray(4);

    case ValueType::VMatrixArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::array<float, 16> matrixValues{};
            if (!reader.ReadBytes(matrixValues.data(), sizeof(float) * matrixValues.size()))
            {
                errorMessage = MakeError("unexpected end while reading matrix array");
                return false;
            }
            attribute.stringArray.push_back(FormatMatrixText(matrixValues));
        }
        return true;

    case ValueType::VoidArray:
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::int32_t byteCount = 0;
            if (!reader.ReadInt32(byteCount) || byteCount < 0)
            {
                errorMessage = MakeError("invalid binary blob array length");
                return false;
            }

            std::vector<std::uint8_t> blob(static_cast<size_t>(byteCount), 0);
            if (byteCount > 0 && !reader.ReadBytes(blob.data(), blob.size()))
            {
                errorMessage = MakeError("unexpected end while reading binary blob array");
                return false;
            }

            attribute.stringArray.push_back(FormatBinaryText(blob));
        }
        return true;

    default:
        errorMessage = MakeError("unsupported array attribute type");
        return false;
    }
}

} // namespace detail

bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage)
{
    using namespace detail;

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

    // v3 uses int16 string count; v4/v5 use int32.
    std::vector<std::string> strings;
    if (encodingVersion <= 3)
    {
        std::int16_t stringCount16 = 0;
        if (!reader.ReadInt16(stringCount16) || stringCount16 < 0)
        {
            errorMessage = MakeError("invalid string table count");
            return false;
        }
        strings.reserve(static_cast<size_t>(stringCount16));
        for (std::int16_t i = 0; i < stringCount16; ++i)
        {
            std::string value;
            if (!reader.ReadCString(value))
            {
                errorMessage = MakeError("unexpected end of string table");
                return false;
            }
            strings.push_back(std::move(value));
        }
    }
    else
    {
        if (!ReadStringTable(reader, strings, errorMessage))
        {
            return false;
        }
    }

    std::int32_t elementCount = 0;
    if (!reader.ReadInt32(elementCount) || elementCount < 0)
    {
        errorMessage = MakeError("invalid element count");
        return false;
    }

    // v3/v4 use int16 for element type/name indices and attribute name indices;
    // v5 uses int32. String values in v3 are inline cstrings (not indices).
    // Helper to read a string table index with the correct width.
    auto readStringIndex = [&](std::int32_t &outIndex) -> bool
    {
        if (encodingVersion <= 4)
        {
            std::int16_t idx16 = -1;
            if (!reader.ReadInt16(idx16))
            {
                return false;
            }
            outIndex = idx16;
        }
        else
        {
            if (!reader.ReadInt32(outIndex))
            {
                return false;
            }
        }
        return true;
    };

    std::vector<std::shared_ptr<Element>> elements(static_cast<size_t>(elementCount));
    for (std::int32_t i = 0; i < elementCount; ++i)
    {
        // v3/v4: int16 typeIndex.  v5: int32 typeIndex.
        std::int32_t typeIndex = -1;
        if (!readStringIndex(typeIndex))
        {
            errorMessage = MakeError("unexpected end while reading element type index");
            return false;
        }

        std::string nameValue;
        if (encodingVersion <= 3)
        {
            // v3: element name is an inline null-terminated string.
            if (!reader.ReadCString(nameValue))
            {
                errorMessage = MakeError("unexpected end while reading element name");
                return false;
            }
        }
        else
        {
            // v4/v5: element name is a string table index.
            std::int32_t nameIndex = -1;
            if (!readStringIndex(nameIndex))
            {
                errorMessage = MakeError("unexpected end while reading element name index");
                return false;
            }
            const std::string *namePtr = LookupString(strings, nameIndex);
            if (!namePtr)
            {
                errorMessage = MakeError("element name index is out of range");
                return false;
            }
            nameValue = *namePtr;
        }

        std::array<std::uint8_t, 16> idBytes{};
        if (!reader.ReadBytes(idBytes.data(), idBytes.size()))
        {
            errorMessage = MakeError("unexpected end while reading element id");
            return false;
        }

        const std::string *typeName = LookupString(strings, typeIndex);
        if (!typeName)
        {
            errorMessage = MakeError("element type index is out of range");
            return false;
        }

        auto element = std::make_shared<Element>();
        element->type = *typeName;
        element->name = nameValue;
        element->id = IdBytesToString(idBytes);

        Attribute nameAttribute;
        nameAttribute.kind = Attribute::Kind::String;
        nameAttribute.declaredType = "string";
        nameAttribute.stringValue = element->name;
        element->attributeOrder.push_back("name");
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
            std::uint8_t attributeTypeCode = 0;
            // v3/v4: attribute name stored as int16 index; v5: int32 index.
            if (!readStringIndex(nameIndex) || !reader.ReadUInt8(attributeTypeCode))
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

            ValueType valueType = ValueType::Unknown;
            if (!TryGetValueTypeFromBinaryTypeCode(attributeTypeCode, valueType))
            {
                errorMessage = MakeError("encountered an unsupported attribute type in binary DMX");
                return false;
            }

            Attribute attribute;
            switch (valueType)
            {
            case ValueType::Element:
                attribute.kind = Attribute::Kind::Element;
                attribute.declaredType = DeclaredTypeFromValueType(valueType);
                if (!ReadElementReference(reader, elements, attribute.elementValue, errorMessage))
                {
                    return false;
                }
                break;

            case ValueType::ElementArray:
            {
                attribute.kind = Attribute::Kind::ElementArray;
                attribute.declaredType = DeclaredTypeFromValueType(valueType);

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

            case ValueType::String:
            {
                attribute.kind = Attribute::Kind::String;
                attribute.declaredType = DeclaredTypeFromValueType(valueType);
                if (encodingVersion <= 3)
                {
                    // v3: string value is an inline null-terminated string.
                    if (!reader.ReadCString(attribute.stringValue))
                    {
                        errorMessage = MakeError("unexpected end while reading string value");
                        return false;
                    }
                }
                else
                {
                    // v4: int16 index; v5: int32 index.
                    std::int32_t valueIndex = -1;
                    if (!readStringIndex(valueIndex))
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
                }
                break;
            }

            case ValueType::Int:
            case ValueType::Float:
            case ValueType::Bool:
            case ValueType::Time:
            case ValueType::Color:
            case ValueType::Vector2:
            case ValueType::Vector3:
            case ValueType::Vector4:
            case ValueType::QAngle:
            case ValueType::Quaternion:
            case ValueType::VMatrix:
            case ValueType::Void:
                attribute.declaredType = DeclaredTypeFromValueType(valueType);
                if (!ReadScalarAttribute(reader, valueType, attribute, errorMessage))
                {
                    return false;
                }
                break;

            case ValueType::IntArray:
            case ValueType::FloatArray:
            case ValueType::BoolArray:
            case ValueType::StringArray:
            case ValueType::TimeArray:
            case ValueType::ColorArray:
            case ValueType::Vector2Array:
            case ValueType::Vector3Array:
            case ValueType::Vector4Array:
            case ValueType::QAngleArray:
            case ValueType::QuaternionArray:
            case ValueType::VMatrixArray:
            case ValueType::VoidArray:
                attribute.declaredType = DeclaredTypeFromValueType(valueType);
                if (!ReadArrayAttribute(reader, valueType, attribute, errorMessage))
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

            if (element->attributes.find(*attributeName) == element->attributes.end())
            {
                element->attributeOrder.push_back(*attributeName);
            }
            element->attributes[*attributeName] = std::move(attribute);
        }
    }

    return true;
}
} // namespace simple_dmx
