#pragma once

#include "GltfExportSession.h"

#include <common_gltf/cgltf.h>

#include <maya/MDagPathArray.h>
#include <maya/MFileObject.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class GltfSceneExporter
{
public:
    GltfSceneExporter(
        const MFileObject &fileObject,
        const GltfExportOptions &exportOptions,
        MPxFileTranslator::FileAccessMode mode);

    MStatus Export();

private:
    // ------- Data structures -------

    struct ExportedMesh
    {
        std::string name;
        std::string parentPath;
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<uint32_t> indices;
        bool hasNormals = false;

        struct ExportedMaterial
        {
            std::string name;
            float baseColorFactor[4] = {0.8f, 0.8f, 0.8f, 1.0f};
            float emissiveFactor[3] = {0.0f, 0.0f, 0.0f};
            float metallicFactor = 0.0f;
            float roughnessFactor = 1.0f;
            float specularFactor = 1.0f;
            float specularColorFactor[3] = {1.0f, 1.0f, 1.0f};
            float ior = 1.5f;
            float transmissionFactor = 0.0f;
            float clearcoatFactor = 0.0f;
            float clearcoatRoughnessFactor = 0.0f;
            float sheenColorFactor[3] = {0.0f, 0.0f, 0.0f};
            float sheenRoughnessFactor = 0.0f;
            std::string baseColorTexturePath;
            std::string metallicRoughnessTexturePath;
            std::string normalTexturePath;
            std::string occlusionTexturePath;
            std::string emissiveTexturePath;
            std::string specularTexturePath;
            std::string specularColorTexturePath;
            std::string transmissionTexturePath;
            std::string clearcoatTexturePath;
            std::string clearcoatRoughnessTexturePath;
            std::string clearcoatNormalTexturePath;
            std::string sheenColorTexturePath;
            std::string sheenRoughnessTexturePath;
            cgltf_alpha_mode alphaMode = cgltf_alpha_mode_opaque;
            float alphaCutoff = 0.5f;
            bool doubleSided = false;
            bool hasPBR = false;
        };
        ExportedMaterial material;

        struct UVSet
        {
            std::string name;
            std::vector<float> data;
        };
        std::vector<UVSet> uvSets;

        std::vector<float> colors;
        bool hasColors = false;
        int colorComponents = 4;

        // Skinning data
        bool hasSkin = false;
        struct SkinJoint
        {
            std::string dagPath;  // full dag path of the joint
            float inverseBindMatrix[16]; // column-major 4x4
        };
        std::vector<SkinJoint> joints;
        std::vector<uint16_t> jointIndices; // glTF JOINTS_* must use u8/u16
        std::vector<float> jointWeights;    // per-vertex, up to 4 per vertex
        int jointsPerVertex = 0;            // max joints affecting one vertex (0, 4, or 8)
    };
    std::vector<ExportedMesh> meshes_;

    struct ExportedCamera
    {
        std::string name;
        std::string nodeDagPath;
        bool isPerspective = true;
        float yfov = 0.785398f;
        float aspectRatio = 0.0f;
        float znear = 0.01f;
        float zfar = 1000.0f;
        float xmag = 1.0f;
        float ymag = 1.0f;
    };
    std::vector<ExportedCamera> cameras_;

    struct ExportedNode
    {
        std::string name;
        std::string dagPath;
        int gltfNodeIndex = -1;
        int parentIndex = -1;
        int meshIndex = -1;
        float translation[3] = {0,0,0};
        float rotation[4] = {0,0,0,1};
        float scale[3] = {1,1,1};
        bool hasTranslation = false;
        bool hasRotation = false;
        bool hasScale = false;
        // Skin index for nodes that have a skin (mesh parent nodes)
        int skinIndex = -1;
    };
    std::vector<ExportedNode> exportedNodes_;
    std::map<std::string, int> dagPathToNodeIndex_;

    std::vector<unsigned char> bufferData_;
    cgltf_data *document_ = nullptr;

    std::vector<cgltf_buffer>      flatBuffers_;
    std::vector<cgltf_buffer_view> flatBufferViews_;
    std::vector<cgltf_accessor>    flatAccessors_;
    std::vector<cgltf_mesh>        flatMeshes_;
    std::vector<cgltf_primitive>   flatPrimitives_;
    std::vector<cgltf_material>    flatMaterials_;
    std::vector<cgltf_texture>     flatTextures_;
    std::vector<cgltf_image>       flatImages_;
    std::vector<cgltf_attribute>   flatAttributes_;
    std::vector<cgltf_node>        flatNodes_;
    std::vector<cgltf_scene>       flatScenes_;
    std::vector<cgltf_node *>      flatSceneNodes_;
    std::vector<std::vector<cgltf_node*>> flatNodeChildren_;
    std::vector<std::vector<cgltf_node*>> flatSkinJointArrays_; // separate to avoid realloc invalidation
    std::vector<cgltf_camera>      flatCameras_;
    std::vector<cgltf_skin>        flatSkins_;

    // Animation data
    struct SampledAnimation
    {
        int targetNodeGltfIndex = -1;   // glTF node this animation targets (set during collect)
        cgltf_animation glAnim;
        std::vector<cgltf_animation_channel> channels;
        std::vector<cgltf_animation_sampler> samplers;
        std::vector<cgltf_accessor> inputAccessors;   // time data per sampler
        std::vector<cgltf_accessor> outputAccessors;  // value data per sampler
        std::vector<cgltf_buffer_view> inputBufferViews;
        std::vector<cgltf_buffer_view> outputBufferViews;
        std::vector<float> inputData;    // flat float storage for all input times
        std::vector<float> outputData;   // flat float storage for all output values
    };
    std::vector<SampledAnimation> sampledAnimations_;

    // Animation flat arrays for cgltf_data document
    std::vector<cgltf_animation>        flatAnimations_;
    std::vector<cgltf_animation_channel> flatAnimChannels_;
    std::vector<cgltf_animation_sampler> flatAnimSamplers_;

    std::vector<char *> ownedStrings_;

    // ------- Methods -------

    MStatus buildBufferData();
    MStatus writeBufferFile(const std::string &binPath);

    MStatus collectMeshes();
    MStatus collectMeshData(const MObject &transformObj);
    MStatus collectSkinData(const MObject &meshTransformObj, ExportedMesh &mesh);
    MStatus collectMaterial(const MObject &transformObj, ExportedMesh::ExportedMaterial &outMat);
    MStatus collectHierarchy(const MDagPath &rootPath);
    int getOrCreateNode(const std::string &dagPath, const std::string &name, int parentIndex);

    // Animation export
    MStatus collectAnimationData();

    MStatus buildDocument(const char *binURIPath);
    void assignDocumentPointers();

    void safeFree();
    static std::string getBasename(const std::string &path);

    // Input
    MFileObject fileObject_;
    GltfExportOptions exportOptions_;
    MPxFileTranslator::FileAccessMode mode_;
};
