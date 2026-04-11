#pragma once

#include <string>
#include <vector>

namespace simple_smd
{
struct Node
{
    int index = -1;
    std::string name;
    int parentIndex = -1;
};

struct SkeletonPose
{
    int boneIndex = -1;
    double tx = 0.0;
    double ty = 0.0;
    double tz = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
};

struct SkeletonFrame
{
    int time = 0;
    std::vector<SkeletonPose> poses;
};

struct TriangleWeight
{
    int boneIndex = -1;
    double weight = 0.0;
};

struct TriangleVertex
{
    int parentBoneIndex = -1;
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double nx = 0.0;
    double ny = 0.0;
    double nz = 0.0;
    double u = 0.0;
    double v = 0.0;
    std::vector<TriangleWeight> links;
};

struct Triangle
{
    std::string materialName;
    std::vector<TriangleVertex> vertices;
};

class Document
{
public:
    bool ParseFromText(const std::string &text, std::string *errorMessage);
    bool ParseFromFile(const std::string &path, std::string *errorMessage);
    std::string Serialize() const;

    int version = 0;
    std::vector<Node> nodes;
    std::vector<SkeletonFrame> skeletonFrames;
    std::vector<Triangle> triangles;
    bool hasVertexAnimation = false;
};
}
