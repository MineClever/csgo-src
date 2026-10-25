#include "GltfMaterialImporter.h"

#include <common/MaterialUtils.h>
#include <common_gltf/MayaGltfCommon.h>
#include <common_gltf/SimpleGltfDocument.h>

#include <maya/MColorArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

// ===========================================================================
// Private constants (moved from anonymous namespace)
// ===========================================================================
namespace {
constexpr const char *kGltfAlphaModeAttribute = "gltfAlphaMode";
constexpr const char *kGltfAlphaCutoffAttribute = "gltfAlphaCutoff";
constexpr const char *kGltfDoubleSidedAttribute = "gltfDoubleSided";
constexpr const char *kGltfOcclusionTextureAttribute = "gltfOcclusionTexture";
}

// ===========================================================================
// Static helpers (moved from anonymous namespace)
// ===========================================================================

bool GltfMaterialImporter::TrySetFloatPlug(MFnDependencyNode &node, const char *plugName, float value)
{
    MStatus status;
    MPlug plug = node.findPlug(plugName, true, &status);
    if (!status) return false;
    return plug.setFloat(value);
}

bool GltfMaterialImporter::TrySetColorPlug(MFnDependencyNode &node, const char *plugName, const float *value)
{
    MStatus status;
    MPlug plug = node.findPlug(plugName, true, &status);
    if (!status || plug.numChildren() < 3) return false;

    status = plug.child(0).setFloat(value[0]);
    status = status ? plug.child(1).setFloat(value[1]) : status;
    status = status ? plug.child(2).setFloat(value[2]) : status;
    return status;
}

bool GltfMaterialImporter::HasAnyNonZeroComponent(const float *value, int count)
{
    for (int index = 0; index < count; ++index)
    {
        if (value[index] > 0.0f) return true;
    }
    return false;
}

MObject GltfMaterialImporter::FindFirstMeshChild(const MObject &transformObject)
{
    MStatus status;
    MFnTransform transformFn(transformObject, &status);
    if (!status) return MObject::kNullObj;

    for (unsigned int index = 0; index < transformFn.childCount(); ++index)
    {
        const MObject child = transformFn.child(index);
        if (child.hasFn(MFn::kMesh))
            return child;
    }
    return MObject::kNullObj;
}

// ===========================================================================
// Constructor
// ===========================================================================

GltfMaterialImporter::GltfMaterialImporter(
    std::shared_ptr<const simple_gltf::Document> document)
    : document_(std::move(document))
{
}

// ===========================================================================
// Helper: resolve image URI from glTF image index
// ===========================================================================

MString GltfMaterialImporter::texturePathFromImageIndex(int imageIndex) const
{
    const cgltf_data *data = document_->GetData();
    if (!data || imageIndex < 0 || imageIndex >= static_cast<int>(data->images_count))
        return MString();
    const cgltf_image &image = data->images[imageIndex];
    return (image.uri && image.uri[0]) ? MString(image.uri) : MString();
}

MPlug GltfMaterialImporter::findShaderPlug(MFnDependencyNode &shaderNode, const char *plugName) const
{
    MStatus plugStatus;
    return shaderNode.findPlug(plugName, true, &plugStatus);
}

// ===========================================================================
// Public API
// ===========================================================================

MStatus GltfMaterialImporter::AssignDefaultMaterial(const MObject &meshTransformObj)
{
    const dcc_material::MaterialNodeNames materialNames =
        dcc_material::BuildMaterialNodeNames("gltf_default", "gltfMaterial");

    MObject shaderObject;
    MObject shadingGroupObject;
    MStatus status = dcc_material::EnsureSurfaceShaderBinding(
        "standardSurface",
        materialNames.shaderName,
        materialNames.shadingGroupName,
        shaderObject,
        shadingGroupObject);

    if (!status)
    {
        return maya_gltf::ReportError("maya_gltf: failed to ensure default shader binding", status);
    }

    status = dcc_material::AssignWholeMeshToShadingGroup(meshTransformObj, shadingGroupObject);
    if (!status)
    {
        return maya_gltf::ReportError("maya_gltf: failed to assign mesh to default shading group", status);
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::AssignMaterial(
    const cgltf_material *material,
    const MObject &meshTransformObj)
{
    if (!material)
    {
        return AssignDefaultMaterial(meshTransformObj);
    }

    const std::string baseName = material->name && material->name[0]
        ? std::string(material->name)
        : "gltf_material";

    const dcc_material::MaterialNodeNames materialNames =
        dcc_material::BuildMaterialNodeNames(baseName, "gltfMaterial");

    MObject shaderObject;
    MObject shadingGroupObject;
    MStatus status = dcc_material::EnsureSurfaceShaderBinding(
        "standardSurface",
        materialNames.shaderName,
        materialNames.shadingGroupName,
        shaderObject,
        shadingGroupObject);

    if (!status)
    {
        return maya_gltf::ReportWarning(
            MString("maya_gltf: failed to ensure shader binding for ") + baseName.c_str());
    }

    MFnDependencyNode shaderNode(shaderObject);

    // --- PBR Metallic-Roughness ---
    status = bindPbrMetallicRoughness(
        material->pbr_metallic_roughness, materialNames, shaderNode);

    // --- Emissive ---
    bindEmissive(material, materialNames, shaderNode);

    // --- Normal texture ---
    bindNormalTexture(material, materialNames, shaderNode);

    // --- Specular extension ---
    if (material->has_specular)
    {
        bindSpecularExtension(material, materialNames, shaderNode);
    }

    // --- Clearcoat extension ---
    if (material->has_clearcoat)
    {
        bindClearcoatExtension(material, materialNames, shaderNode);
    }

    // --- Sheen extension ---
    if (material->has_sheen)
    {
        bindSheenExtension(material, materialNames, shaderNode);
    }

    // --- Transmission extension ---
    if (material->has_transmission)
    {
        bindTransmissionExtension(material, materialNames, shaderNode);
    }

    // --- IOR ---
    if (material->has_ior)
    {
        TrySetFloatPlug(shaderNode, "specularIOR", material->ior.ior);
    }

    // --- Occlusion texture (stored as attribute) ---
    if (material->occlusion_texture.texture && material->occlusion_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->occlusion_texture.texture->image - document_->GetData()->images);
        dcc_material::EnsureStringAttribute(
            shaderObject,
            kGltfOcclusionTextureAttribute,
            texturePathFromImageIndex(imgIdx));
    }

    // --- Alpha mode / double-sided attributes ---
    {
        const char *alphaMode = "OPAQUE";
        if (material->alpha_mode == cgltf_alpha_mode_mask)
            alphaMode = "MASK";
        else if (material->alpha_mode == cgltf_alpha_mode_blend)
            alphaMode = "BLEND";

        dcc_material::EnsureStringAttribute(shaderObject, kGltfAlphaModeAttribute, alphaMode);
        dcc_material::EnsureFloatAttribute(shaderObject, kGltfAlphaCutoffAttribute, material->alpha_cutoff);
        dcc_material::EnsureBoolAttribute(shaderObject, kGltfDoubleSidedAttribute, material->double_sided != 0);
    }

    // --- Double-sided mesh shape ---
    {
        const MObject meshShapeObject = FindFirstMeshChild(meshTransformObj);
        if (!meshShapeObject.isNull())
        {
            MFnDependencyNode meshNode(meshShapeObject);
            MStatus doubleSidedStatus;
            MPlug doubleSidedPlug = meshNode.findPlug("doubleSided", true, &doubleSidedStatus);
            if (doubleSidedStatus)
            {
                doubleSidedPlug.setBool(material->double_sided != 0);
            }
        }
    }

    // --- Assign to shading group ---
    status = dcc_material::AssignWholeMeshToShadingGroup(meshTransformObj, shadingGroupObject);
    if (!status)
    {
        return maya_gltf::ReportWarning(
            MString("maya_gltf: failed to assign mesh to shading group for ") + baseName.c_str());
    }

    return MS::kSuccess;
}

// ===========================================================================
// PBR binding implementations
// ===========================================================================

MStatus GltfMaterialImporter::bindPbrMetallicRoughness(
    const cgltf_pbr_metallic_roughness &pbr,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    const cgltf_data *data = document_->GetData();

    if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(
            pbr.base_color_texture.texture->image - data->images);
        const MPlug baseColorPlug = findShaderPlug(shaderNode, "baseColor");
        if (!baseColorPlug.isNull())
        {
            dcc_material::AssignFileTextureToPlug(
                names.shaderName + "_baseColorFile",
                texturePathFromImageIndex(imgIdx),
                baseColorPlug,
                false);
        }
    }
    else
    {
        const float *color = pbr.base_color_factor;
        TrySetColorPlug(shaderNode, "baseColor", color);
    }

    if (pbr.base_color_texture.texture == nullptr || pbr.base_color_texture.texture->image == nullptr)
    {
        const float opacity[3] = { pbr.base_color_factor[3], pbr.base_color_factor[3], pbr.base_color_factor[3] };
        TrySetColorPlug(shaderNode, "opacity", opacity);
    }
    TrySetFloatPlug(shaderNode, "metalness", pbr.metallic_factor);
    TrySetFloatPlug(shaderNode, "specularRoughness", pbr.roughness_factor);

    if (pbr.metallic_roughness_texture.texture && pbr.metallic_roughness_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(
            pbr.metallic_roughness_texture.texture->image - data->images);
        const MString texturePath = texturePathFromImageIndex(imgIdx);
        const MPlug metalnessPlug = findShaderPlug(shaderNode, "metalness");
        if (!metalnessPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_metallicRoughnessFile",
                texturePath,
                metalnessPlug,
                dcc_material::TextureChannel::Blue,
                dcc_material::TextureColorSpace::Raw);
        }

        const MPlug roughnessPlug = findShaderPlug(shaderNode, "specularRoughness");
        if (!roughnessPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_metallicRoughnessFile",
                texturePath,
                roughnessPlug,
                dcc_material::TextureChannel::Green,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindEmissive(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    if (HasAnyNonZeroComponent(material->emissive_factor, 3))
    {
        TrySetFloatPlug(shaderNode, "emission", 1.0f);
        TrySetColorPlug(shaderNode, "emissionColor", material->emissive_factor);
    }

    if (material->emissive_texture.texture && material->emissive_texture.texture->image)
    {
        const cgltf_data *data = document_->GetData();
        const int imgIdx = static_cast<int>(material->emissive_texture.texture->image - data->images);
        const MPlug emissionColorPlug = findShaderPlug(shaderNode, "emissionColor");
        if (!emissionColorPlug.isNull())
        {
            TrySetFloatPlug(shaderNode, "emission", 1.0f);
            dcc_material::AssignFileTextureToPlug(
                names.shaderName + "_emissiveFile",
                texturePathFromImageIndex(imgIdx),
                emissionColorPlug,
                false);
        }
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindNormalTexture(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    if (material->normal_texture.texture && material->normal_texture.texture->image)
    {
        const cgltf_data *data = document_->GetData();
        const int imgIdx = static_cast<int>(material->normal_texture.texture->image - data->images);
        const MPlug normalPlug = findShaderPlug(shaderNode, "normalCamera");
        if (!normalPlug.isNull())
        {
            dcc_material::AssignNormalTextureToShader(
                names.shaderName,
                texturePathFromImageIndex(imgIdx),
                normalPlug);
        }
    }
    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindSpecularExtension(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    const cgltf_data *data = document_->GetData();

    TrySetFloatPlug(shaderNode, "specular", material->specular.specular_factor);
    TrySetColorPlug(shaderNode, "specularColor", material->specular.specular_color_factor);

    if (material->specular.specular_texture.texture && material->specular.specular_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->specular.specular_texture.texture->image - data->images);
        const MPlug specularPlug = findShaderPlug(shaderNode, "specular");
        if (!specularPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_specularFile",
                texturePathFromImageIndex(imgIdx),
                specularPlug,
                dcc_material::TextureChannel::Alpha,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    if (material->specular.specular_color_texture.texture && material->specular.specular_color_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->specular.specular_color_texture.texture->image - data->images);
        const MPlug specularColorPlug = findShaderPlug(shaderNode, "specularColor");
        if (!specularColorPlug.isNull())
        {
            dcc_material::AssignFileTextureToPlug(
                names.shaderName + "_specularColorFile",
                texturePathFromImageIndex(imgIdx),
                specularColorPlug,
                false);
        }
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindClearcoatExtension(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    const cgltf_data *data = document_->GetData();

    TrySetFloatPlug(shaderNode, "coat", material->clearcoat.clearcoat_factor);
    TrySetFloatPlug(shaderNode, "coatRoughness", material->clearcoat.clearcoat_roughness_factor);

    if (material->clearcoat.clearcoat_texture.texture && material->clearcoat.clearcoat_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->clearcoat.clearcoat_texture.texture->image - data->images);
        const MPlug coatPlug = findShaderPlug(shaderNode, "coat");
        if (!coatPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_clearcoatFile",
                texturePathFromImageIndex(imgIdx),
                coatPlug,
                dcc_material::TextureChannel::Red,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    if (material->clearcoat.clearcoat_roughness_texture.texture && material->clearcoat.clearcoat_roughness_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->clearcoat.clearcoat_roughness_texture.texture->image - data->images);
        const MPlug coatRoughnessPlug = findShaderPlug(shaderNode, "coatRoughness");
        if (!coatRoughnessPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_clearcoatRoughnessFile",
                texturePathFromImageIndex(imgIdx),
                coatRoughnessPlug,
                dcc_material::TextureChannel::Green,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    if (material->clearcoat.clearcoat_normal_texture.texture && material->clearcoat.clearcoat_normal_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->clearcoat.clearcoat_normal_texture.texture->image - data->images);
        const MPlug coatNormalPlug = findShaderPlug(shaderNode, "coatNormal");
        if (!coatNormalPlug.isNull())
        {
            dcc_material::AssignNormalTextureToShader(
                names.shaderName + "_clearcoat",
                texturePathFromImageIndex(imgIdx),
                coatNormalPlug);
        }
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindSheenExtension(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    const cgltf_data *data = document_->GetData();

    TrySetColorPlug(shaderNode, "sheenColor", material->sheen.sheen_color_factor);
    TrySetFloatPlug(shaderNode, "sheenRoughness", material->sheen.sheen_roughness_factor);

    if (material->sheen.sheen_color_texture.texture && material->sheen.sheen_color_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->sheen.sheen_color_texture.texture->image - data->images);
        const MPlug sheenColorPlug = findShaderPlug(shaderNode, "sheenColor");
        if (!sheenColorPlug.isNull())
        {
            dcc_material::AssignFileTextureToPlug(
                names.shaderName + "_sheenColorFile",
                texturePathFromImageIndex(imgIdx),
                sheenColorPlug,
                false);
        }
    }

    if (material->sheen.sheen_roughness_texture.texture && material->sheen.sheen_roughness_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->sheen.sheen_roughness_texture.texture->image - data->images);
        const MPlug sheenRoughnessPlug = findShaderPlug(shaderNode, "sheenRoughness");
        if (!sheenRoughnessPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_sheenRoughnessFile",
                texturePathFromImageIndex(imgIdx),
                sheenRoughnessPlug,
                dcc_material::TextureChannel::Alpha,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    return MS::kSuccess;
}

MStatus GltfMaterialImporter::bindTransmissionExtension(
    const cgltf_material *material,
    const dcc_material::MaterialNodeNames &names,
    MFnDependencyNode &shaderNode)
{
    const cgltf_data *data = document_->GetData();

    TrySetFloatPlug(shaderNode, "transmission", material->transmission.transmission_factor);

    if (material->transmission.transmission_texture.texture && material->transmission.transmission_texture.texture->image)
    {
        const int imgIdx = static_cast<int>(material->transmission.transmission_texture.texture->image - data->images);
        const MPlug transmissionPlug = findShaderPlug(shaderNode, "transmission");
        if (!transmissionPlug.isNull())
        {
            dcc_material::AssignTextureChannelToPlug(
                names.shaderName + "_transmissionFile",
                texturePathFromImageIndex(imgIdx),
                transmissionPlug,
                dcc_material::TextureChannel::Red,
                dcc_material::TextureColorSpace::Raw);
        }
    }

    return MS::kSuccess;
}
