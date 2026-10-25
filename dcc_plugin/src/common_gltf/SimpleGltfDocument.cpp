#include "SimpleGltfDocument.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <cstring>

namespace simple_gltf
{

Document::Document()  = default;
Document::~Document() { Free(); }

Document::Document(Document &&other) noexcept
    : data_(other.data_)
{
    other.data_ = nullptr;
}

Document &Document::operator=(Document &&other) noexcept
{
    if (this != &other)
    {
        Free();
        data_ = other.data_;
        other.data_ = nullptr;
    }
    return *this;
}

void Document::Free()
{
    if (data_)
    {
        cgltf_free(data_);
        data_ = nullptr;
    }
}

bool Document::ParseFromFile(const char *filePath, std::string *errorMessage)
{
    Free();

    cgltf_options options = {};
    // auto-detect type (gltf vs glb)
    options.type = cgltf_file_type_invalid;

    cgltf_result result = cgltf_parse_file(&options, filePath, &data_);

    if (result != cgltf_result_success)
    {
        if (errorMessage)
        {
            switch (result)
            {
            case cgltf_result_file_not_found:
                *errorMessage = "File not found: " + std::string(filePath);
                break;
            case cgltf_result_io_error:
                *errorMessage = "I/O error reading: " + std::string(filePath);
                break;
            case cgltf_result_out_of_memory:
                *errorMessage = "Out of memory while parsing: " + std::string(filePath);
                break;
            case cgltf_result_unknown_format:
                *errorMessage = "Unknown or unsupported format: " + std::string(filePath);
                break;
            case cgltf_result_invalid_json:
                *errorMessage = "Invalid JSON in glTF file: " + std::string(filePath);
                break;
            case cgltf_result_invalid_gltf:
                *errorMessage = "Invalid glTF data: " + std::string(filePath);
                break;
            case cgltf_result_legacy_gltf:
                *errorMessage = "Legacy glTF 1.0 format (only 2.0 is supported): " + std::string(filePath);
                break;
            default:
                *errorMessage = "Unknown parse error (" + std::to_string(static_cast<int>(result)) + "): " + std::string(filePath);
                break;
            }
        }
        return false;
    }

    // Load external buffer files (data URIs and GLB-embedded buffers are also handled)
    result = cgltf_load_buffers(&options, data_, filePath);
    if (result != cgltf_result_success)
    {
        if (errorMessage)
        {
            *errorMessage = "Failed to load buffer data from: " + std::string(filePath);
        }
        Free();
        return false;
    }

    return true;
}

} // namespace simple_gltf
