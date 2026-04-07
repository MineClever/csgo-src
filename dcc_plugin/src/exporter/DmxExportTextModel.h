#pragma once

#include <deque>
#include <sstream>
#include <string>
#include <vector>

namespace dmx_export
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

class DmxTextBuilder
{
public:
    DmxElement *CreateElement(const std::string &type);
    std::string Serialize(const DmxElement &root) const;

private:
    static std::string Indent(int level);
    static void WriteQuoted(std::ostringstream &stream, const std::string &value);
    static void WriteElement(std::ostringstream &stream, const DmxElement &element, int indentLevel);
    static void WriteElementBody(std::ostringstream &stream, const DmxElement &element, int indentLevel);
    static void WriteAttribute(std::ostringstream &stream, const DmxAttribute &attribute, int indentLevel);

    std::deque<DmxElement> m_elements;
    int m_nextId = 0;
};

const DmxAttribute *FindAttribute(const DmxElement &element, const char *attributeName);
std::string GetElementName(const DmxElement &element);
DmxAttribute MakeScalarAttribute(const std::string &name, const std::string &type, const std::string &value);
DmxAttribute MakeScalarArrayAttribute(const std::string &name, const std::string &type, std::vector<std::string> values);
DmxAttribute MakeInlineElementAttribute(const std::string &name, DmxElement *element);
DmxAttribute MakeElementArrayAttribute(const std::string &name, const std::vector<DmxElement *> &elements);
DmxElement *CloneElement(DmxTextBuilder &builder, const DmxElement &source);
}
