#pragma once

#include "SimpleDmxDocument.h"

#include <string>

namespace simple_dmx
{
bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage);
}
