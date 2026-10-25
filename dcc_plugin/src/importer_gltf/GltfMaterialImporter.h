#pragma once

#include <common_gltf/cgltf.h>

#include <memory>

#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace simple_gltf { class Document; }
struct cgltf_material;

// Forward declaration from MaterialUtils.h
namespace dcc_material { struct MaterialNodeNames; }

// ---------------------------------------------------------------------------
// Imports glTF materials into Maya standardSurface shaders.
// Encapsulates all PBR texture binding patterns to eliminate repeated
// texture-lookup code across extension handling (specular, clearcoat, sheen,
// transmission, etc.).
// ---------------------------------------------------------------------------
class GltfMaterialImporter
{
public:
    explicit GltfMaterialImporter(std::shared_ptr<const simple_gltf::Document> document);

    MStatus AssignDefaultMaterial(const MObject &meshTransformObj);
    MStatus AssignMaterial(const cgltf_material *material, const MObject &meshTransformObj);

private:
    std::shared_ptr<const simple_gltf::Document> document_;

    // --- PBR binding helpers (extracted from repeated patterns) ---
    MStatus bindPbrMetallicRoughness(
        const cgltf_pbr_metallic_roughness &pbr,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindEmissive(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindNormalTexture(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindSpecularExtension(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindClearcoatExtension(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindSheenExtension(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    MStatus bindTransmissionExtension(
        const cgltf_material *material,
        const dcc_material::MaterialNodeNames &names,
        MFnDependencyNode &shaderNode);

    // --- Internal helpers ---
    MString texturePathFromImageIndex(int imageIndex) const;
    MPlug findShaderPlug(MFnDependencyNode &shaderNode, const char *plugName) const;

    // Small utility helpers moved from anonymous namespace
    static bool TrySetFloatPlug(MFnDependencyNode &node, const char *plugName, float value);
    static bool TrySetColorPlug(MFnDependencyNode &node, const char *plugName, const float *value);
    static bool HasAnyNonZeroComponent(const float *value, int count);
    static MObject FindFirstMeshChild(const MObject &transformObject);
};
