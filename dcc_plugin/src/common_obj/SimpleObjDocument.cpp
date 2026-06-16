#include "SimpleObjDocument.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace simple_obj
{

namespace
{

// Read the file once. If it contains NUL bytes (common in older ZBrush and
// some DCC exports), write a NUL-free copy to a temporary file and return its
// path. Returns empty path when no cleaning is needed or on I/O failure.
std::string WriteCleanedTemp(const char *filePath, bool &outOk)
{
    outOk = true;

    std::ifstream in(filePath, std::ios::binary);
    if (!in)
    {
        outOk = false;
        return {};
    }

    const auto tempPath = (std::filesystem::temp_directory_path() / "rapidobj_clean.obj").string();
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        outOk = false;
        return {};
    }

    bool hasNul = false;
    char ch;
    while (in.get(ch))
    {
        if (ch == '\0')
        {
            hasNul = true;
            continue;
        }
        out.put(ch);
    }

    out.close();

    if (!hasNul)
    {
        std::filesystem::remove(tempPath);
        return {};
    }

    return tempPath;
}

} // namespace

bool Document::ParseFromFile(const char *filePath, std::string *errorMessage)
{
    const std::filesystem::path objPath(filePath);
    const rapidobj::MaterialLibrary mtlLibrary =
        rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional);

    // Fast path: memory-mapped ParseFile for the vast majority of files
    result_ = rapidobj::ParseFile(objPath, mtlLibrary);

    // Fallback: some files contain embedded NUL bytes that trip up the parser.
    // Read once, strip NUL, write a clean temp file, and re-parse.
    if (result_.error)
    {
        bool ok = true;
        const std::string tempPath = WriteCleanedTemp(filePath, ok);

        if (!ok)
        {
            if (errorMessage)
            {
                *errorMessage = "Cannot open file: " + std::string(filePath);
            }
            return false;
        }

        if (!tempPath.empty())
        {
            result_ = rapidobj::ParseFile(std::filesystem::path(tempPath), mtlLibrary);
            std::filesystem::remove(tempPath);
        }
    }

    if (result_.error)
    {
        if (errorMessage)
        {
            *errorMessage = result_.error.code.message();
            if (!result_.error.line.empty())
            {
                *errorMessage += " near line " + std::to_string(result_.error.line_num)
                    + ": " + result_.error.line;
            }
        }
        return false;
    }

    // Do NOT triangulate — preserve the original polygon structure
    // (quads, n-gons) so the importer can create native Maya polygons.

    return true;
}

} // namespace simple_obj
