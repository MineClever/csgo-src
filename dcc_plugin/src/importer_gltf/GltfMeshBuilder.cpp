#include "GltfMeshBuilder.h"
#include "GltfAccessorReader.h"

#include <common_gltf/MayaGltfCommon.h>

#include <algorithm>
#include <cstdint>

GltfMeshBuilder::GltfMeshBuilder(const cgltf_primitive &primitive)
    : primitive_(primitive)
{
}

bool GltfMeshBuilder::collectAccessors(
    const cgltf_accessor *&posAccessor,
    const cgltf_accessor *&nrmAccessor,
    std::vector<std::pair<int, const cgltf_accessor*>> &uvAccessors,
    const cgltf_accessor *&colorAccessor) const
{
    posAccessor = nullptr;
    nrmAccessor = nullptr;
    uvAccessors.clear();
    colorAccessor = nullptr;

    for (cgltf_size i = 0; i < primitive_.attributes_count; ++i)
    {
        const cgltf_attribute &attr = primitive_.attributes[i];
        switch (attr.type)
        {
        case cgltf_attribute_type_position:
            posAccessor = attr.data;
            break;
        case cgltf_attribute_type_normal:
            nrmAccessor = attr.data;
            break;
        case cgltf_attribute_type_texcoord:
        {
            const int setIdx = static_cast<int>(attr.index);
            if (setIdx < 8)
                uvAccessors.push_back({setIdx, attr.data});
        }
            break;
        case cgltf_attribute_type_color:
            if (attr.index == 0)
                colorAccessor = attr.data;
            break;
        default:
            break;
        }
    }

    return posAccessor != nullptr;
}

bool GltfMeshBuilder::Build(GltfMeshData &outData)
{
    const cgltf_accessor *posAccessor = nullptr;
    const cgltf_accessor *nrmAccessor = nullptr;
    std::vector<std::pair<int, const cgltf_accessor*>> uvAccessors;
    const cgltf_accessor *colorAccessor = nullptr;

    if (!collectAccessors(posAccessor, nrmAccessor, uvAccessors, colorAccessor))
    {
        maya_gltf::ReportWarning("maya_gltf: primitive has no position accessor, skipping");
        return false;
    }

    // Sort UV accessors by set index
    std::sort(uvAccessors.begin(), uvAccessors.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });

    // Read raw accessor data
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<std::vector<float>> uvData;
    std::vector<uint32_t> indices;

    {
        GltfAccessorReader posReader(posAccessor);
        if (!posReader.ReadFloats(positions, 3))
        {
            maya_gltf::ReportWarning("maya_gltf: failed to read position accessor");
            return false;
        }
    }

    const bool hasNormals = nrmAccessor && [&]() {
        GltfAccessorReader nrmReader(nrmAccessor);
        return nrmReader.ReadFloats(normals, 3);
    }();

    for (auto &uvPair : uvAccessors)
    {
        std::vector<float> uvFloats;
        GltfAccessorReader uvReader(uvPair.second);
        if (uvReader.ReadFloats(uvFloats, 2))
            uvData.push_back(std::move(uvFloats));
    }

    std::vector<float> colors;
    const int colorComponents = colorAccessor ? static_cast<int>(cgltf_num_components(colorAccessor->type)) : 0;
    const bool hasColors = colorAccessor && colorComponents >= 3 && [&]() {
        GltfAccessorReader colorReader(colorAccessor);
        return colorReader.ReadFloats(colors, 3);
    }();

    const bool indexed = primitive_.indices != nullptr;
    if (indexed)
    {
        GltfAccessorReader idxReader(primitive_.indices);
        if (!idxReader.ReadIndices(indices))
        {
            maya_gltf::ReportWarning("maya_gltf: failed to read index accessor");
            return false;
        }
    }

    const cgltf_size vertexCount = posAccessor->count;
    const cgltf_size primCount = indexed ? indices.size() : vertexCount;

    if (primCount % 3 != 0)
    {
        maya_gltf::ReportWarning("maya_gltf: primitive vertex count is not a multiple of 3 — may not be triangulated");
    }

    const int numFaces = static_cast<int>(primCount / 3);
    int mayaVertexIndex = 0;
    auto &vertexMap = outData.vertexMap;

    outData.uvSets.resize(uvData.size());

    for (int faceIdx = 0; faceIdx < numFaces; ++faceIdx)
    {
        outData.polygonCounts.append(3);

        for (int v = 0; v < 3; ++v)
        {
            const int gltfVertIdx = indexed
                ? static_cast<int>(indices[faceIdx * 3 + v])
                : faceIdx * 3 + v;

            if (gltfVertIdx < 0 || static_cast<cgltf_size>(gltfVertIdx) >= vertexCount)
                continue;

            int mayaIdx;
            const auto it = vertexMap.find(gltfVertIdx);
            if (it != vertexMap.end())
            {
                mayaIdx = it->second;
            }
            else
            {
                mayaIdx = mayaVertexIndex++;
                vertexMap[gltfVertIdx] = mayaIdx;

                const float *p = &positions[gltfVertIdx * 3];
                outData.vertexArray.append(MFloatPoint(p[0], p[1], p[2]));

                for (size_t u = 0; u < uvData.size(); ++u)
                {
                    auto &uvSet = outData.uvSets[u];
                    if (uvSet.name.length() == 0)
                    {
                        uvSet.name = (u == 0) ? MString("map1") : MString("map") + MString() + static_cast<int>(u + 1);
                    }
                    if ((gltfVertIdx * 2 + 1) < static_cast<int>(uvData[u].size()))
                    {
                        uvSet.uArray.append(uvData[u][gltfVertIdx * 2]);
                        uvSet.vArray.append(1.0f - uvData[u][gltfVertIdx * 2 + 1]);
                    }
                    else
                    {
                        uvSet.uArray.append(0.0f);
                        uvSet.vArray.append(0.0f);
                    }
                }

                if (hasColors && ((gltfVertIdx * colorComponents) + colorComponents - 1) < static_cast<int>(colors.size()))
                {
                    const float *c = &colors[gltfVertIdx * colorComponents];
                    const float alpha = (colorComponents >= 4) ? c[3] : 1.0f;
                    outData.colorArray.append(MColor(c[0], c[1], c[2], alpha));
                    outData.hasColors = true;
                }
            }

            outData.polygonConnects.append(mayaIdx);

            for (size_t u = 0; u < outData.uvSets.size(); ++u)
            {
                outData.uvSets[u].uvIds.append(mayaIdx);
            }

            if (hasNormals && (gltfVertIdx * 3 + 2) < static_cast<int>(normals.size()))
            {
                const float *n = &normals[gltfVertIdx * 3];
                outData.normalArray.append(MVector(n[0], n[1], n[2]));
                outData.normalFaceList.append(faceIdx);
                outData.normalVertexList.append(v);
            }
        }
    }

    return true;
}
