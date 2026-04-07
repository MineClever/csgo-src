#include "DmxImportUtils.h"

#include <algorithm>
#include <sstream>

namespace dmx_import_utils
{
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

    std::istringstream stream(normalized);
    std::vector<double> values;
    double value = 0.0;
    while (stream >> value)
    {
        values.push_back(value);
    }

    return values;
}

const simple_dmx::Element *FindAttributeElement(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName)
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

std::vector<const simple_dmx::Element *> FindAttributeElementArray(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName)
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

std::string FindAttributeString(const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != simple_dmx::Attribute::Kind::String)
    {
        return {};
    }

    return it->second.stringValue;
}

std::vector<std::string> FindAttributeStringArray(const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != simple_dmx::Attribute::Kind::StringArray)
    {
        return {};
    }

    return it->second.stringArray;
}

std::string SanitizeNodeName(const std::string &name)
{
    std::string sanitized = name.empty() ? "dmxMaterial" : name;
    for (char &ch : sanitized)
    {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_';
        if (!ok)
        {
            ch = '_';
        }
    }
    return sanitized;
}
}
