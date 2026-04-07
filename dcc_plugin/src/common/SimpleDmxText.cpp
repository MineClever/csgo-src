#include "SimpleDmxText.h"
#include "SimpleDmxBinary.h"
#include "SimpleDmxTypes.h"

#include <cctype>
#include <sstream>

namespace simple_dmx
{
class Parser
{
public:
    explicit Parser(const std::string &text, Document &document)
        : m_text(text), m_document(document)
    {
    }

    bool Parse(std::string &errorMessage)
    {
        SkipWhitespaceAndComments();
        while (!IsAtEnd())
        {
            std::shared_ptr<Element> element;
            if (!ParseElement(element, errorMessage))
            {
                return false;
            }

            if (!m_document.m_root)
            {
                m_document.m_root = element;
            }

            SkipWhitespaceAndComments();
        }

        if (!m_document.m_root)
        {
            errorMessage = "No DMX element found in file.";
            return false;
        }

        return true;
    }

private:
    void RecordAttributeOrder(Element &element, const std::string &attributeName) const
    {
        if (element.attributes.find(attributeName) == element.attributes.end())
        {
            element.attributeOrder.push_back(attributeName);
        }
    }

    bool ParseElement(std::shared_ptr<Element> &element, std::string &errorMessage, const std::string *forcedType = nullptr)
    {
        std::string typeName;
        if (forcedType)
        {
            typeName = *forcedType;
        }
        else if (!ParseQuotedString(typeName, errorMessage))
        {
            return false;
        }

        if (!Consume('{', errorMessage, "Expected '{' after element type."))
        {
            return false;
        }

        element = std::make_shared<Element>();
        element->type = typeName;
        m_document.m_ownedElements.push_back(element);

        while (true)
        {
            SkipWhitespaceAndComments();
            if (IsAtEnd())
            {
                errorMessage = "Unexpected end of file while reading element body.";
                return false;
            }

            if (Peek() == '}')
            {
                Advance();
                break;
            }

            std::string attributeName;
            std::string attributeType;
            if (!ParseQuotedString(attributeName, errorMessage))
            {
                return false;
            }
            if (!ParseQuotedString(attributeType, errorMessage))
            {
                return false;
            }

            if (attributeType == "elementid")
            {
                std::string id;
                if (!ParseQuotedString(id, errorMessage))
                {
                    return false;
                }

                element->id = id;
                m_document.m_elementsById[id] = element.get();
                continue;
            }

            Attribute attribute;
            attribute.declaredType = attributeType;

            if (attributeType == "element")
            {
                attribute.kind = Attribute::Kind::Element;
                if (!ParseQuotedString(attribute.elementValue.referenceId, errorMessage))
                {
                    return false;
                }
            }
            else if (attributeType == "element_array")
            {
                attribute.kind = Attribute::Kind::ElementArray;
                if (!ParseElementArray(attribute, errorMessage))
                {
                    return false;
                }
            }
            else if (IsArrayValueType(ValueTypeFromDeclaredType(attributeType)))
            {
                attribute.kind = Attribute::Kind::StringArray;
                if (!ParseStringArray(attribute.stringArray, errorMessage))
                {
                    return false;
                }
            }
            else if (IsScalarValueType(ValueTypeFromDeclaredType(attributeType)))
            {
                attribute.kind = Attribute::Kind::String;
                if (!ParseQuotedString(attribute.stringValue, errorMessage))
                {
                    return false;
                }
            }
            else
            {
                SkipWhitespaceAndComments();
                if (IsAtEnd())
                {
                    errorMessage = "Unexpected end of file while reading unknown attribute type.";
                    return false;
                }

                if (Peek() == '"')
                {
                    attribute.kind = Attribute::Kind::String;
                    if (!ParseQuotedString(attribute.stringValue, errorMessage))
                    {
                        return false;
                    }
                }
                else if (Peek() == '[')
                {
                    attribute.kind = Attribute::Kind::StringArray;
                    if (!ParseStringArray(attribute.stringArray, errorMessage))
                    {
                        return false;
                    }
                }
                else if (Peek() == '{')
                {
                    attribute.kind = Attribute::Kind::Element;
                    if (!ParseElement(attribute.elementValue.inlineElement, errorMessage, &attributeType))
                    {
                        return false;
                    }
                }
                else
                {
                    errorMessage = "Unsupported attribute payload for unknown DMX attribute type.";
                    return false;
                }
            }

            if (attributeName == "name" && attribute.kind == Attribute::Kind::String)
            {
                element->name = attribute.stringValue;
            }

            RecordAttributeOrder(*element, attributeName);
            element->attributes.emplace(attributeName, std::move(attribute));
        }

        if (element->name.empty())
        {
            auto it = element->attributes.find("name");
            if (it != element->attributes.end() && it->second.kind == Attribute::Kind::String)
            {
                element->name = it->second.stringValue;
            }
        }

        return true;
    }

    bool ParseElementArray(Attribute &attribute, std::string &errorMessage)
    {
        if (!Consume('[', errorMessage, "Expected '[' at the start of an element array."))
        {
            return false;
        }

        while (true)
        {
            SkipWhitespaceAndComments();
            if (IsAtEnd())
            {
                errorMessage = "Unexpected end of file while reading element array.";
                return false;
            }

            if (Peek() == ']')
            {
                Advance();
                break;
            }

            std::string itemType;
            if (!ParseQuotedString(itemType, errorMessage))
            {
                return false;
            }

            ElementLink link;
            if (itemType == "element")
            {
                if (!ParseQuotedString(link.referenceId, errorMessage))
                {
                    return false;
                }
            }
            else
            {
                if (!ParseElement(link.inlineElement, errorMessage, &itemType))
                {
                    return false;
                }
            }

            attribute.elementArray.push_back(std::move(link));

            SkipWhitespaceAndComments();
            if (!IsAtEnd() && Peek() == ',')
            {
                Advance();
            }
        }

        return true;
    }

    bool ParseStringArray(std::vector<std::string> &values, std::string &errorMessage)
    {
        if (!Consume('[', errorMessage, "Expected '[' at the start of an array attribute."))
        {
            return false;
        }

        while (true)
        {
            SkipWhitespaceAndComments();
            if (IsAtEnd())
            {
                errorMessage = "Unexpected end of file while reading array attribute.";
                return false;
            }

            if (Peek() == ']')
            {
                Advance();
                break;
            }

            std::string value;
            if (!ParseQuotedString(value, errorMessage))
            {
                return false;
            }

            values.push_back(std::move(value));

            SkipWhitespaceAndComments();
            if (!IsAtEnd() && Peek() == ',')
            {
                Advance();
            }
        }

        return true;
    }

    bool ParseQuotedString(std::string &value, std::string &errorMessage)
    {
        SkipWhitespaceAndComments();
        if (IsAtEnd() || Peek() != '"')
        {
            errorMessage = "Expected quoted string token.";
            return false;
        }

        Advance();
        value.clear();

        while (!IsAtEnd())
        {
            const char current = Advance();
            if (current == '"')
            {
                return true;
            }

            if (current == '\\' && !IsAtEnd())
            {
                value.push_back(Advance());
                continue;
            }

            value.push_back(current);
        }

        errorMessage = "Unterminated quoted string.";
        return false;
    }

    bool Consume(char expected, std::string &errorMessage, const char *message)
    {
        SkipWhitespaceAndComments();
        if (IsAtEnd() || Peek() != expected)
        {
            errorMessage = message;
            return false;
        }

        Advance();
        return true;
    }

    void SkipWhitespaceAndComments()
    {
        while (!IsAtEnd())
        {
            if (std::isspace(static_cast<unsigned char>(Peek())) != 0)
            {
                Advance();
                continue;
            }

            if (Peek() == '/' && PeekNext() == '/')
            {
                while (!IsAtEnd() && Peek() != '\n')
                {
                    Advance();
                }
                continue;
            }

            if (Peek() == '<' && MatchSequence("<!--"))
            {
                while (!IsAtEnd() && !MatchSequence("-->"))
                {
                    Advance();
                }
                if (!IsAtEnd())
                {
                    Advance();
                    Advance();
                    Advance();
                }
                continue;
            }

            break;
        }
    }

    bool MatchSequence(const char *sequence) const
    {
        for (size_t i = 0; sequence[i] != '\0'; ++i)
        {
            if (m_position + i >= m_text.size() || m_text[m_position + i] != sequence[i])
            {
                return false;
            }
        }

        return true;
    }

    bool IsAtEnd() const
    {
        return m_position >= m_text.size();
    }

    char Peek() const
    {
        return IsAtEnd() ? '\0' : m_text[m_position];
    }

    char PeekNext() const
    {
        return (m_position + 1) < m_text.size() ? m_text[m_position + 1] : '\0';
    }

    char Advance()
    {
        return m_text[m_position++];
    }

    const std::string &m_text;
    Document &m_document;
    size_t m_position = 0;
};

bool Document::Parse(const std::string &text, std::string &errorMessage)
{
    m_root.reset();
    m_ownedElements.clear();
    m_elementsById.clear();

    Parser parser(text, *this);
    return parser.Parse(errorMessage);
}

const Element *Document::GetRoot() const
{
    return m_root.get();
}

const Element *Document::ResolveElement(const Attribute &attribute) const
{
    if (attribute.kind != Attribute::Kind::Element)
    {
        return nullptr;
    }

    if (attribute.elementValue.inlineElement)
    {
        return attribute.elementValue.inlineElement.get();
    }

    auto it = m_elementsById.find(attribute.elementValue.referenceId);
    return it != m_elementsById.end() ? it->second : nullptr;
}

std::vector<const Element *> Document::ResolveElementArray(const Attribute &attribute) const
{
    std::vector<const Element *> result;
    if (attribute.kind != Attribute::Kind::ElementArray)
    {
        return result;
    }

    for (const ElementLink &link : attribute.elementArray)
    {
        if (link.inlineElement)
        {
            result.push_back(link.inlineElement.get());
            continue;
        }

        auto it = m_elementsById.find(link.referenceId);
        if (it != m_elementsById.end())
        {
            result.push_back(it->second);
        }
    }

    return result;
}

bool IsBinaryDmx(const std::string &bytes)
{
    if (bytes.rfind("<!-- dmx ", 0) != 0)
    {
        return false;
    }

    const size_t headerEnd = bytes.find("-->");
    if (headerEnd == std::string::npos)
    {
        return false;
    }

    const std::string header = bytes.substr(0, headerEnd + 3);
    return header.find("encoding binary ") != std::string::npos;
}

bool ParseDocument(const std::string &bytes, Document &document, std::string &errorMessage)
{
    if (IsBinaryDmx(bytes))
    {
        return ParseBinaryDocument(bytes, document, errorMessage);
    }

    return document.Parse(bytes, errorMessage);
}
}
