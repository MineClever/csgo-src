#pragma once

#include "rapidobj.hpp"

#include <string>

namespace simple_obj
{

// Thin wrapper around rapidobj::Result that provides file-parsing entry points
// matching the SimpleSmdDocument / SimpleDmxDocument pattern.
class Document
{
public:
    // Parse an OBJ file from disk. On parse or triangulation failure, returns false
    // and writes a diagnostic to errorMessage (when non-null).
    bool ParseFromFile(const char *filePath, std::string *errorMessage);

    const rapidobj::Result &GetResult() const { return result_; }
    rapidobj::Result &GetResult() { return result_; }

private:
    rapidobj::Result result_;
};

} // namespace simple_obj
