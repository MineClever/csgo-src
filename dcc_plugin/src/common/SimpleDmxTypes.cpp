#include "SimpleDmxTypes.h"

namespace simple_dmx
{
namespace
{
struct ValueTypeInfo
{
    ValueType valueType;
    const char *declaredType;
    bool isScalar;
    bool isArray;
    int componentCount;
    int binaryTypeCode;
};

constexpr ValueTypeInfo kValueTypeInfos[] = {
    {ValueType::Element, "element", false, false, 0, 1},
    {ValueType::Int, "int", true, false, 1, 2},
    {ValueType::Float, "float", true, false, 1, 3},
    {ValueType::Bool, "bool", true, false, 1, 4},
    {ValueType::String, "string", true, false, 1, 5},
    {ValueType::Time, "time", true, false, 1, -1},
    {ValueType::Color, "color", true, false, 4, -1},
    {ValueType::Vector2, "vector2", true, false, 2, 9},
    {ValueType::Vector3, "vector3", true, false, 3, 10},
    {ValueType::Vector4, "vector4", true, false, 4, 11},
    {ValueType::QAngle, "qangle", true, false, 3, -1},
    {ValueType::Quaternion, "quaternion", true, false, 4, 13},
    {ValueType::VMatrix, "vmatrix", true, false, 16, -1},
    {ValueType::Void, "void", true, false, 0, -1},
    {ValueType::ElementArray, "element_array", false, false, 0, 15},
    {ValueType::IntArray, "int_array", false, true, 1, 16},
    {ValueType::FloatArray, "float_array", false, true, 1, 17},
    {ValueType::BoolArray, "bool_array", false, true, 1, -1},
    {ValueType::StringArray, "string_array", false, true, 1, 19},
    {ValueType::TimeArray, "time_array", false, true, 1, -1},
    {ValueType::ColorArray, "color_array", false, true, 4, -1},
    {ValueType::Vector2Array, "vector2_array", false, true, 2, 23},
    {ValueType::Vector3Array, "vector3_array", false, true, 3, 24},
    {ValueType::Vector4Array, "vector4_array", false, true, 4, 25},
    {ValueType::QAngleArray, "qangle_array", false, true, 3, -1},
    {ValueType::QuaternionArray, "quaternion_array", false, true, 4, 27},
    {ValueType::VMatrixArray, "vmatrix_array", false, true, 16, -1},
    {ValueType::VoidArray, "void_array", false, true, 0, -1},
};

const ValueTypeInfo *FindValueTypeInfo(ValueType valueType)
{
    for (const ValueTypeInfo &info : kValueTypeInfos)
    {
        if (info.valueType == valueType)
        {
            return &info;
        }
    }
    return nullptr;
}

const ValueTypeInfo *FindValueTypeInfo(const std::string &declaredType)
{
    for (const ValueTypeInfo &info : kValueTypeInfos)
    {
        if (declaredType == info.declaredType)
        {
            return &info;
        }
    }
    return nullptr;
}

const ValueTypeInfo *FindValueTypeInfo(std::uint8_t binaryTypeCode)
{
    for (const ValueTypeInfo &info : kValueTypeInfos)
    {
        if (info.binaryTypeCode == static_cast<int>(binaryTypeCode))
        {
            return &info;
        }
    }
    return nullptr;
}
}

ValueType ValueTypeFromDeclaredType(const std::string &declaredType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(declaredType);
    return info ? info->valueType : ValueType::Unknown;
}

const char *DeclaredTypeFromValueType(ValueType valueType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(valueType);
    return info ? info->declaredType : "";
}

bool IsScalarValueType(ValueType valueType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(valueType);
    return info ? info->isScalar : false;
}

bool IsArrayValueType(ValueType valueType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(valueType);
    return info ? info->isArray : false;
}

int ComponentCountForValueType(ValueType valueType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(valueType);
    return info ? info->componentCount : 0;
}

bool TryGetBinaryTypeCode(ValueType valueType, std::uint8_t &typeCode)
{
    const ValueTypeInfo *info = FindValueTypeInfo(valueType);
    if (!info || info->binaryTypeCode < 0)
    {
        return false;
    }

    typeCode = static_cast<std::uint8_t>(info->binaryTypeCode);
    return true;
}

bool TryGetValueTypeFromBinaryTypeCode(std::uint8_t typeCode, ValueType &valueType)
{
    const ValueTypeInfo *info = FindValueTypeInfo(typeCode);
    if (!info)
    {
        valueType = ValueType::Unknown;
        return false;
    }

    valueType = info->valueType;
    return true;
}
}
