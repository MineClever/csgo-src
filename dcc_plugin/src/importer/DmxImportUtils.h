#pragma once

#include "../common/SimpleDmxDocument.h"

#include <string>
#include <vector>

namespace dmx_import_utils
{
std::vector<double> ParseNumberList(const std::string &text);
const simple_dmx::Element *FindAttributeElement(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName);
std::vector<const simple_dmx::Element *> FindAttributeElementArray(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName);
std::string FindAttributeString(const simple_dmx::Element *element, const char *attributeName);
std::vector<std::string> FindAttributeStringArray(const simple_dmx::Element *element, const char *attributeName);
std::string SanitizeNodeName(const std::string &name);
}
