#include "SimpleDmxDocument.h"

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
