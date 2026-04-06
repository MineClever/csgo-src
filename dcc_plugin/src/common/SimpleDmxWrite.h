#pragma once

#include "SimpleDmxText.h"

#include <string>

namespace simple_dmx
{
std::string SerializeDocumentText(const Document &document);
bool SerializeDocumentBinary(const Document &document, std::string &output, std::string &errorMessage);
}
