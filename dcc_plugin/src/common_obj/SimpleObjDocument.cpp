#include "SimpleObjDocument.h"

#include <filesystem>

namespace simple_obj
{

bool Document::ParseFromFile(const char *filePath, std::string *errorMessage)
{
    const std::filesystem::path objPath(filePath);

    // Load material library (optional — MTL files may not exist next to every OBJ)
    const rapidobj::MaterialLibrary mtlLibrary =
        rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional);

    result_ = rapidobj::ParseFile(objPath, mtlLibrary);

    if (result_.error)
    {
        if (errorMessage)
        {
            *errorMessage = result_.error.code.message();
            if (!result_.error.line.empty())
            {
                *errorMessage += " near line " + std::to_string(result_.error.line_num) + ": " + result_.error.line;
            }
        }
        return false;
    }

    // Do NOT triangulate — preserve the original polygon structure
    // (quads, n-gons) so the importer can create native Maya polygons.

    return true;
}

} // namespace simple_obj
