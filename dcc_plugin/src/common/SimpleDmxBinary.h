#pragma once

#include "SimpleDmxText.h"

#include <string>

namespace simple_dmx
{
bool ParseBinaryDocument(const std::string &bytes, Document &document, std::string &errorMessage);
}
