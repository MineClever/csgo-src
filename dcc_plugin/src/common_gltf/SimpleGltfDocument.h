#pragma once

#include <string>

// cgltf is included in the .cpp only — clients use GetData() to access the parsed result.
struct cgltf_data;

namespace simple_gltf
{

// RAII wrapper around cgltf_data that provides file-parsing entry points
// matching the SimpleDmxDocument / SimpleObjDocument pattern.
class Document
{
public:
    Document();
    ~Document();

    // Non-copyable
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    // Movable
    Document(Document &&other) noexcept;
    Document &operator=(Document &&other) noexcept;

    // Parse a glTF or GLB file from disk. On failure returns false and
    // writes a diagnostic to errorMessage (when non-null).
    bool ParseFromFile(const char *filePath, std::string *errorMessage);

    // Access the underlying cgltf_data (may be null on error or after move).
    const cgltf_data *GetData() const { return data_; }
    cgltf_data *GetData() { return data_; }

    // Convenience: returns true when data_ is non-null and valid.
    bool IsValid() const { return data_ != nullptr; }

private:
    void Free();

    cgltf_data *data_ = nullptr;
};

} // namespace simple_gltf
