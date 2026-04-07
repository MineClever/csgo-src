#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace simple_dmx
{
class Parser;
struct Element;

struct ElementLink
{
    std::string referenceId;
    std::shared_ptr<Element> inlineElement;
};

struct Attribute
{
    enum class Kind
    {
        String,
        StringArray,
        Element,
        ElementArray,
    };

    Kind kind = Kind::String;
    std::string declaredType;
    std::string stringValue;
    std::vector<std::string> stringArray;
    ElementLink elementValue;
    std::vector<ElementLink> elementArray;
};

struct Element
{
    std::string type;
    std::string id;
    std::string name;
    std::unordered_map<std::string, Attribute> attributes;
    std::vector<std::string> attributeOrder;
};

class Document
{
public:
    bool Parse(const std::string &text, std::string &errorMessage);

    const Element *GetRoot() const;
    const Element *ResolveElement(const Attribute &attribute) const;
    std::vector<const Element *> ResolveElementArray(const Attribute &attribute) const;

private:
    std::shared_ptr<Element> m_root;
    std::vector<std::shared_ptr<Element>> m_ownedElements;
    std::unordered_map<std::string, const Element *> m_elementsById;

    friend class Parser;
    friend bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage);
};
}
