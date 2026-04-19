#pragma once

#include <cstdint>
#include <string>

namespace simple_dmx
{
enum class ValueType
{
    Unknown = 0,
    Element,
    Int,
    Float,
    Bool,
    String,
    Time,
    Color,
    Vector2,
    Vector3,
    Vector4,
    QAngle,
    Quaternion,
    VMatrix,
    Void,
    ElementArray,
    IntArray,
    FloatArray,
    BoolArray,
    StringArray,
    TimeArray,
    ColorArray,
    Vector2Array,
    Vector3Array,
    Vector4Array,
    QAngleArray,
    QuaternionArray,
    VMatrixArray,
    VoidArray,
};

ValueType ValueTypeFromDeclaredType(const std::string &declaredType);
const char *DeclaredTypeFromValueType(ValueType valueType);
bool IsScalarValueType(ValueType valueType);
bool IsArrayValueType(ValueType valueType);
int ComponentCountForValueType(ValueType valueType);

bool TryGetBinaryTypeCode(ValueType valueType, std::uint8_t &typeCode);
bool TryGetValueTypeFromBinaryTypeCode(std::uint8_t typeCode, ValueType &valueType);
}
