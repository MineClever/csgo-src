#include "GltfAccessorReader.h"

GltfAccessorReader::GltfAccessorReader(const cgltf_accessor *accessor)
    : accessor_(accessor)
{
}

bool GltfAccessorReader::ReadFloats(std::vector<float> &out, cgltf_size expectedComponents) const
{
    if (!accessor_) return false;

    const cgltf_size count = accessor_->count;
    const cgltf_size numComponents = cgltf_num_components(accessor_->type);
    if (numComponents < expectedComponents) return false;

    out.resize(count * numComponents);
    const cgltf_size written = cgltf_accessor_unpack_floats(accessor_, out.data(), out.size());
    return written == out.size();
}

bool GltfAccessorReader::ReadUInts(std::vector<cgltf_uint> &out, cgltf_size expectedComponents) const
{
    if (!accessor_) return false;

    const cgltf_size count = accessor_->count;
    const cgltf_size numComponents = cgltf_num_components(accessor_->type);
    if (numComponents < expectedComponents || numComponents > 4) return false;

    out.resize(count * numComponents);
    std::vector<cgltf_uint> element(numComponents, 0);
    for (cgltf_size index = 0; index < count; ++index)
    {
        if (!cgltf_accessor_read_uint(accessor_, index, element.data(), numComponents))
        {
            return false;
        }

        for (cgltf_size component = 0; component < numComponents; ++component)
        {
            out[index * numComponents + component] = element[component];
        }
    }

    return true;
}

bool GltfAccessorReader::ReadIndices(std::vector<uint32_t> &out) const
{
    if (!accessor_) return false;

    const cgltf_size count = accessor_->count;
    out.resize(count);

    for (cgltf_size i = 0; i < count; ++i)
    {
        out[i] = static_cast<uint32_t>(cgltf_accessor_read_index(accessor_, i));
    }

    return true;
}
