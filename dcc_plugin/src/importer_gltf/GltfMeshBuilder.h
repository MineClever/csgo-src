#pragma once

#include <common_gltf/cgltf.h>

#include <map>
#include <vector>

#include <maya/MColorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

// ---------------------------------------------------------------------------
// Output data structure: a Maya-friendly single-vertex mesh built from a
// glTF primitive (positions, normals, UVs, colors, deduplicated).
// ---------------------------------------------------------------------------
struct GltfMeshData
{
    MFloatPointArray vertexArray;
    MIntArray polygonCounts;
    MIntArray polygonConnects;

    struct UVSetData
    {
        MString name;
        MFloatArray uArray;
        MFloatArray vArray;
        MIntArray uvIds;
    };
    std::vector<UVSetData> uvSets;

    MColorArray colorArray;
    bool hasColors = false;

    MVectorArray normalArray;
    MIntArray normalFaceList;
    MIntArray normalVertexList;

    // glTF vertex index → Maya vertex index (from deduplication)
    std::map<int, int> vertexMap;
};

// ---------------------------------------------------------------------------
// Builds GltfMeshData from a single glTF primitive.
// ---------------------------------------------------------------------------
class GltfMeshBuilder
{
public:
    explicit GltfMeshBuilder(const cgltf_primitive &primitive);

    bool Build(GltfMeshData &outData);

private:
    const cgltf_primitive &primitive_;

    bool collectAccessors(
        const cgltf_accessor *&posAccessor,
        const cgltf_accessor *&nrmAccessor,
        std::vector<std::pair<int, const cgltf_accessor*>> &uvAccessors,
        const cgltf_accessor *&colorAccessor) const;
};
