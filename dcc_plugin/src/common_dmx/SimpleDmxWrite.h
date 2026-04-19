#pragma once

#include "SimpleDmxDocument.h"

#include <string>

namespace simple_dmx
{
std::string SerializeDocumentText(const Document &document);
bool SerializeDocumentBinary(const Document &document, std::string &output, std::string &errorMessage);
}
