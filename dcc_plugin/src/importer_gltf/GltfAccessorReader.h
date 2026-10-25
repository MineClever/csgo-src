#pragma once

#include <common_gltf/cgltf.h>

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// OOP wrapper around a cgltf_accessor pointer.
// Eliminates repeated "const cgltf_accessor *accessor" parameters across
// the read helpers by binding the accessor at construction time.
// ---------------------------------------------------------------------------
class GltfAccessorReader
{
public:
    explicit GltfAccessorReader(const cgltf_accessor *accessor);

    // Returns false on null/insufficient-component accessors.
    bool ReadFloats(std::vector<float> &out, cgltf_size expectedComponents) const;
    bool ReadUInts(std::vector<cgltf_uint> &out, cgltf_size expectedComponents) const;
    bool ReadIndices(std::vector<uint32_t> &out) const;

    cgltf_size Count() const { return accessor_ ? accessor_->count : 0; }
    cgltf_size NumComponents() const { return accessor_ ? cgltf_num_components(accessor_->type) : 0; }
    bool IsValid() const { return accessor_ != nullptr; }

    const cgltf_accessor *Accessor() const { return accessor_; }

private:
    const cgltf_accessor *accessor_;
};
