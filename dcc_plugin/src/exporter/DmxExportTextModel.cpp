#include "DmxExportTextModel.h"

#include <utility>

namespace dmx_export
{
DmxElement *DmxTextBuilder::CreateElement(const std::string &type)
{
    m_elements.push_back({});
    DmxElement &element = m_elements.back();
    element.type = type;
    element.id = "id_" + std::to_string(++m_nextId);
    return &element;
}

std::string DmxTextBuilder::Serialize(const DmxElement &root) const
{
    std::ostringstream stream;
    stream << "<!-- dmx encoding keyvalues2 1 format model 1 -->\n";
    WriteElement(stream, root, 0);
    return stream.str();
}

std::string DmxTextBuilder::Indent(int level)
{
    return std::string(level * 4, ' ');
}

void DmxTextBuilder::WriteQuoted(std::ostringstream &stream, const std::string &value)
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

void DmxTextBuilder::WriteElement(std::ostringstream &stream, const DmxElement &element, int indentLevel)
{
    stream << Indent(indentLevel);
    WriteQuoted(stream, element.type);
    stream << "\n";
    WriteElementBody(stream, element, indentLevel);
}

void DmxTextBuilder::WriteElementBody(std::ostringstream &stream, const DmxElement &element, int indentLevel)
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

void DmxTextBuilder::WriteAttribute(std::ostringstream &stream, const DmxAttribute &attribute, int indentLevel)
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

DmxElement *CloneElement(DmxTextBuilder &builder, const DmxElement &source)
{
    DmxElement *clone = builder.CreateElement(source.type);
    clone->attributes = source.attributes;
    return clone;
}
}
