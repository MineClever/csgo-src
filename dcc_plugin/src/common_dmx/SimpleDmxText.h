#pragma once

#include "SimpleDmxDocument.h"

#include <string>

namespace simple_dmx
{
bool ParseDocument(const std::string &bytes, Document &document, std::string &errorMessage);
bool IsBinaryDmx(const std::string &bytes);
}
