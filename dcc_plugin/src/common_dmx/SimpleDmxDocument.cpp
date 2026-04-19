#include "SimpleDmxDocument.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace simple_dmx
{

Attribute ScalarAttr(std::string declaredType, std::string value)
{
    Attribute attr;
    attr.kind = Attribute::Kind::String;
    attr.declaredType = std::move(declaredType);
    attr.stringValue = std::move(value);
    return attr;
}

Attribute ScalarArrayAttr(std::string declaredType, std::vector<std::string> values)
{
    Attribute attr;
    attr.kind = Attribute::Kind::StringArray;
    attr.declaredType = std::move(declaredType);
    attr.stringArray = std::move(values);
    return attr;
}

void SetAttr(Element &element, std::string name, Attribute attr)
{
    if (element.attributes.find(name) == element.attributes.end())
    {
        element.attributeOrder.push_back(name);
    }
    element.attributes[std::move(name)] = std::move(attr);
}

void ClearAttrs(Element &element)
{
    element.attributes.clear();
    element.attributeOrder.clear();
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

const Element *FindAttributeElement(const Document &document, const Element *element, const char *attributeName)
{
    if (!element)
    {
        return nullptr;
    }
    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end())
    {
        return nullptr;
    }
    return document.ResolveElement(it->second);
}

std::vector<const Element *> FindAttributeElementArray(const Document &document, const Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }
    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end())
    {
        return {};
    }
    return document.ResolveElementArray(it->second);
}

std::string FindAttributeString(const Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }
    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != Attribute::Kind::String)
    {
        return {};
    }
    return it->second.stringValue;
}

std::vector<std::string> FindAttributeStringArray(const Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }
    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != Attribute::Kind::StringArray)
    {
        return {};
    }
    return it->second.stringArray;
}

Element *DocumentBuilder::CreateElement(const std::string &type, const std::string &name)
{
    auto el = std::make_shared<Element>();
    el->type = type;
    el->name = name;
    el->id = "id_" + std::to_string(++m_nextId);
    Element *ptr = el.get();
    m_ptrToShared[ptr] = el;
    m_owned.push_back(std::move(el));
    return ptr;
}

void DocumentBuilder::SetRoot(Element *root)
{
    m_root = root;
}

Attribute DocumentBuilder::ElementRef(Element *element)
{
    Attribute attr;
    attr.kind = Attribute::Kind::Element;
    if (element)
    {
        auto it = m_ptrToShared.find(element);
        if (it != m_ptrToShared.end())
        {
            attr.elementValue.inlineElement = it->second;
        }
    }
    return attr;
}

Attribute DocumentBuilder::ElementRefArray(const std::vector<Element *> &elements)
{
    Attribute attr;
    attr.kind = Attribute::Kind::ElementArray;
    for (Element *element : elements)
    {
        ElementLink link;
        if (element)
        {
            auto it = m_ptrToShared.find(element);
            if (it != m_ptrToShared.end())
            {
                link.inlineElement = it->second;
            }
        }
        attr.elementArray.push_back(std::move(link));
    }
    return attr;
}

Document DocumentBuilder::Build()
{
    Document doc;
    doc.m_ownedElements.assign(m_owned.begin(), m_owned.end());
    for (const auto &el : doc.m_ownedElements)
    {
        if (!el->id.empty())
        {
            doc.m_elementsById[el->id] = el.get();
        }
    }
    if (m_root)
    {
        auto it = m_ptrToShared.find(m_root);
        if (it != m_ptrToShared.end())
        {
            doc.m_root = it->second;
        }
    }
    return doc;
}

}
