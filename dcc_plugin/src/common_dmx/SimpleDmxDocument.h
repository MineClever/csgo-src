#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace simple_dmx
{
class Parser;
class DocumentBuilder;
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
    friend class DocumentBuilder;
    friend bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage);
};

// --- Attribute construction helpers ---

// Create a scalar string attribute (e.g. "float", "vector3", "string", ...).
Attribute ScalarAttr(std::string declaredType, std::string value);

// Create a scalar array attribute (e.g. "float_array", "int_array", ...).
Attribute ScalarArrayAttr(std::string declaredType, std::vector<std::string> values);

// Add (or replace) a named attribute on an element, maintaining attributeOrder.
void SetAttr(Element &element, std::string name, Attribute attr);

// Clear all attributes from an element (map + order).
void ClearAttrs(Element &element);

// --- DOM query helpers ---

// Parse a whitespace/comma/bracket-delimited list of numbers into a double vector.
std::vector<double> ParseNumberList(const std::string &text);

// Return the single Element referenced by a named attribute, or nullptr.
const Element *FindAttributeElement(const Document &document, const Element *element, const char *attributeName);

// Return all Elements referenced by a named element-array attribute.
std::vector<const Element *> FindAttributeElementArray(const Document &document, const Element *element, const char *attributeName);

// Return the string value of a named scalar attribute, or empty string.
std::string FindAttributeString(const Element *element, const char *attributeName);

// Return the string array of a named array attribute, or empty vector.
std::vector<std::string> FindAttributeStringArray(const Element *element, const char *attributeName);

// --- DocumentBuilder ---
// Builds a Document programmatically without parsing text/binary.
// All element links use inlineElement so SerializeDocumentText produces the
// standard nested keyvalues2 format (same style as the old DmxTextBuilder).
class DocumentBuilder
{
public:
    // Create a new owned element.  'name' sets Element::name directly.
    Element *CreateElement(const std::string &type, const std::string &name = {});

    // Designate the root element (must have been created by this builder).
    void SetRoot(Element *root);

    // Build an Element-kind Attribute that references 'element' as an inline child.
    Attribute ElementRef(Element *element);

    // Build an ElementArray-kind Attribute with inline references to each element.
    Attribute ElementRefArray(const std::vector<Element *> &elements);

    // Consume the builder and return the final Document.
    Document Build();

private:
    std::deque<std::shared_ptr<Element>> m_owned;
    std::unordered_map<Element *, std::shared_ptr<Element>> m_ptrToShared;
    Element *m_root = nullptr;
    int m_nextId = 0;
};

}
