#include "SimpleObjDocument.h"

#include <filesystem>

namespace simple_obj
{

bool Document::ParseFromFile(const char *filePath, std::string *errorMessage)
{
    const std::filesystem::path objPath(filePath);

    // Load default material library handling (search next to .obj, mandatory load is default)
    const rapidobj::MaterialLibrary mtlLibrary = rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional);

    result_ = rapidobj::ParseFile(objPath, mtlLibrary);

    if (result_.error)
    {
        if (errorMessage)
        {
            *errorMessage = result_.error.code.message();
        }
        return false;
    }

    // Triangulate non-triangular faces so the scene importer only deals with triangles
    if (!rapidobj::Triangulate(result_))
    {
        if (errorMessage)
        {
            *errorMessage = "Triangulation failed for: " + std::string(filePath);
        }
        return false;
    }

    return true;
}

} // namespace simple_obj
