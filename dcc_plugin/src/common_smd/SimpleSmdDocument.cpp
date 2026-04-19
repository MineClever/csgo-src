#include "SimpleSmdDocument.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace simple_smd
{
namespace simple_smd_impl
{
std::string Trim(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

bool StartsWith(const std::string &value, const char *prefix)
{
    const std::string prefixString(prefix);
    return value.size() >= prefixString.size() && value.compare(0, prefixString.size(), prefixString) == 0;
}

bool ParseNodeLine(const std::string &line, Node &node)
{
    const size_t firstQuote = line.find('"');
    const size_t secondQuote = line.find('"', firstQuote == std::string::npos ? firstQuote : firstQuote + 1);
    if (firstQuote == std::string::npos || secondQuote == std::string::npos || secondQuote <= firstQuote)
    {
        return false;
    }

    std::istringstream prefixStream(Trim(line.substr(0, firstQuote)));
    if (!(prefixStream >> node.index))
    {
        return false;
    }

    node.name = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);

    std::istringstream suffixStream(Trim(line.substr(secondQuote + 1)));
    return static_cast<bool>(suffixStream >> node.parentIndex);
}

bool ParseSkeletonPoseLine(const std::string &line, SkeletonPose &pose)
{
    std::istringstream stream(line);
    return static_cast<bool>(
        stream >> pose.boneIndex >> pose.tx >> pose.ty >> pose.tz >> pose.rx >> pose.ry >> pose.rz);
}

bool ParseTriangleVertexLine(const std::string &line, TriangleVertex &vertex)
{
    std::istringstream stream(line);
    if (!(stream >> vertex.parentBoneIndex >> vertex.px >> vertex.py >> vertex.pz >> vertex.nx >> vertex.ny >> vertex.nz >> vertex.u >> vertex.v))
    {
        return false;
    }

    int linkCount = 0;
    if (!(stream >> linkCount))
    {
        return true;
    }

    for (int linkIndex = 0; linkIndex < linkCount; ++linkIndex)
    {
        TriangleWeight weight;
        if (!(stream >> weight.boneIndex >> weight.weight))
        {
            return false;
        }
        vertex.links.push_back(weight);
    }

    return true;
}

bool ParseVertexAnimationSampleLine(const std::string &line, VertexAnimationSample &sample)
{
    std::istringstream stream(line);
    return static_cast<bool>(
        stream >> sample.vertexIndex >>
        sample.px >> sample.py >> sample.pz >>
        sample.nx >> sample.ny >> sample.nz);
}

void AppendLine(std::ostringstream &stream, const std::string &line)
{
    stream << line << '\n';
}
}

bool Document::ParseFromText(const std::string &text, std::string *errorMessage)
{
    version = 0;
    nodes.clear();
    skeletonFrames.clear();
    triangles.clear();
    vertexAnimationFrames.clear();
    hasVertexAnimation = false;

    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        const std::string trimmed = simple_smd_impl::Trim(line);
        if (trimmed.empty() || simple_smd_impl::StartsWith(trimmed, "//"))
        {
            continue;
        }

        if (simple_smd_impl::StartsWith(trimmed, "version"))
        {
            std::istringstream stream(trimmed);
            std::string keyword;
            if (!(stream >> keyword >> version))
            {
                if (errorMessage)
                {
                    *errorMessage = "Failed to parse version line";
                }
                return false;
            }
            continue;
        }

        if (trimmed == "nodes")
        {
            while (std::getline(input, line))
            {
                ++lineNumber;
                const std::string nodeLine = simple_smd_impl::Trim(line);
                if (nodeLine.empty() || simple_smd_impl::StartsWith(nodeLine, "//"))
                {
                    continue;
                }
                if (nodeLine == "end")
                {
                    break;
                }

                Node node;
                if (!simple_smd_impl::ParseNodeLine(nodeLine, node))
                {
                    if (errorMessage)
                    {
                        *errorMessage = "Failed to parse nodes line at " + std::to_string(lineNumber);
                    }
                    return false;
                }
                nodes.push_back(node);
            }
            continue;
        }

        if (trimmed == "skeleton")
        {
            SkeletonFrame *currentFrame = nullptr;
            while (std::getline(input, line))
            {
                ++lineNumber;
                const std::string skeletonLine = simple_smd_impl::Trim(line);
                if (skeletonLine.empty() || simple_smd_impl::StartsWith(skeletonLine, "//"))
                {
                    continue;
                }
                if (skeletonLine == "end")
                {
                    break;
                }

                if (simple_smd_impl::StartsWith(skeletonLine, "time"))
                {
                    std::istringstream stream(skeletonLine);
                    std::string keyword;
                    SkeletonFrame frame;
                    if (!(stream >> keyword >> frame.time))
                    {
                        if (errorMessage)
                        {
                            *errorMessage = "Failed to parse skeleton time line at " + std::to_string(lineNumber);
                        }
                        return false;
                    }
                    skeletonFrames.push_back(frame);
                    currentFrame = &skeletonFrames.back();
                    continue;
                }

                if (!currentFrame)
                {
                    if (errorMessage)
                    {
                        *errorMessage = "Encountered skeleton pose before time block at " + std::to_string(lineNumber);
                    }
                    return false;
                }

                SkeletonPose pose;
                if (!simple_smd_impl::ParseSkeletonPoseLine(skeletonLine, pose))
                {
                    if (errorMessage)
                    {
                        *errorMessage = "Failed to parse skeleton pose line at " + std::to_string(lineNumber);
                    }
                    return false;
                }
                currentFrame->poses.push_back(pose);
            }
            continue;
        }

        if (trimmed == "triangles")
        {
            while (std::getline(input, line))
            {
                ++lineNumber;
                const std::string materialLine = simple_smd_impl::Trim(line);
                if (materialLine.empty() || simple_smd_impl::StartsWith(materialLine, "//"))
                {
                    continue;
                }
                if (materialLine == "end")
                {
                    break;
                }

                Triangle triangle;
                triangle.materialName = materialLine;
                for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
                {
                    if (!std::getline(input, line))
                    {
                        if (errorMessage)
                        {
                            *errorMessage = "Unexpected EOF while reading triangle vertices";
                        }
                        return false;
                    }

                    ++lineNumber;
                    TriangleVertex vertex;
                    if (!simple_smd_impl::ParseTriangleVertexLine(simple_smd_impl::Trim(line), vertex))
                    {
                        if (errorMessage)
                        {
                            *errorMessage = "Failed to parse triangle vertex line at " + std::to_string(lineNumber);
                        }
                        return false;
                    }
                    triangle.vertices.push_back(vertex);
                }
                triangles.push_back(triangle);
            }
            continue;
        }

        if (trimmed == "vertexanimation")
        {
            hasVertexAnimation = true;
            VertexAnimationFrame *currentFrame = nullptr;
            while (std::getline(input, line))
            {
                ++lineNumber;
                const std::string vertexAnimationLine = simple_smd_impl::Trim(line);
                if (vertexAnimationLine.empty() || simple_smd_impl::StartsWith(vertexAnimationLine, "//"))
                {
                    continue;
                }

                if (vertexAnimationLine == "end")
                {
                    break;
                }

                if (simple_smd_impl::StartsWith(vertexAnimationLine, "time"))
                {
                    std::istringstream stream(vertexAnimationLine);
                    std::string keyword;
                    VertexAnimationFrame frame;
                    if (!(stream >> keyword >> frame.time))
                    {
                        if (errorMessage)
                        {
                            *errorMessage = "Failed to parse vertexanimation time line at " + std::to_string(lineNumber);
                        }
                        return false;
                    }

                    vertexAnimationFrames.push_back(frame);
                    currentFrame = &vertexAnimationFrames.back();
                    continue;
                }

                if (!currentFrame)
                {
                    if (errorMessage)
                    {
                        *errorMessage = "Encountered vertexanimation sample before time block at " + std::to_string(lineNumber);
                    }
                    return false;
                }

                VertexAnimationSample sample;
                if (!simple_smd_impl::ParseVertexAnimationSampleLine(vertexAnimationLine, sample))
                {
                    if (errorMessage)
                    {
                        *errorMessage = "Failed to parse vertexanimation sample line at " + std::to_string(lineNumber);
                    }
                    return false;
                }
                currentFrame->samples.push_back(sample);
            }
            continue;
        }
    }

    return true;
}

bool Document::ParseFromFile(const std::string &path, std::string *errorMessage)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        if (errorMessage)
        {
            *errorMessage = "Failed to open file: " + path;
        }
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return ParseFromText(buffer.str(), errorMessage);
}

std::string Document::Serialize() const
{
    std::ostringstream output;
    simple_smd_impl::AppendLine(output, "version " + std::to_string(version <= 0 ? 1 : version));

    simple_smd_impl::AppendLine(output, "nodes");
    for (const Node &node : nodes)
    {
        simple_smd_impl::AppendLine(output, "  " + std::to_string(node.index) + " \"" + node.name + "\" " + std::to_string(node.parentIndex));
    }
    simple_smd_impl::AppendLine(output, "end");

    simple_smd_impl::AppendLine(output, "skeleton");
    for (const SkeletonFrame &frame : skeletonFrames)
    {
        simple_smd_impl::AppendLine(output, "time " + std::to_string(frame.time));
        for (const SkeletonPose &pose : frame.poses)
        {
            std::ostringstream poseLine;
            poseLine << "  " << pose.boneIndex << ' '
                     << pose.tx << ' ' << pose.ty << ' ' << pose.tz << ' '
                     << pose.rx << ' ' << pose.ry << ' ' << pose.rz;
            simple_smd_impl::AppendLine(output, poseLine.str());
        }
    }
    simple_smd_impl::AppendLine(output, "end");

    if (!triangles.empty())
    {
        simple_smd_impl::AppendLine(output, "triangles");
        for (const Triangle &triangle : triangles)
        {
            simple_smd_impl::AppendLine(output, triangle.materialName);
            for (const TriangleVertex &vertex : triangle.vertices)
            {
                std::ostringstream vertexLine;
                vertexLine << vertex.parentBoneIndex << ' '
                           << vertex.px << ' ' << vertex.py << ' ' << vertex.pz << ' '
                           << vertex.nx << ' ' << vertex.ny << ' ' << vertex.nz << ' '
                           << vertex.u << ' ' << vertex.v;
                if (!vertex.links.empty())
                {
                    vertexLine << ' ' << static_cast<int>(vertex.links.size());
                    for (const TriangleWeight &weight : vertex.links)
                    {
                        vertexLine << ' ' << weight.boneIndex << ' ' << weight.weight;
                    }
                }
                simple_smd_impl::AppendLine(output, vertexLine.str());
            }
        }
        simple_smd_impl::AppendLine(output, "end");
    }

    if (!vertexAnimationFrames.empty())
    {
        simple_smd_impl::AppendLine(output, "vertexanimation");
        for (const VertexAnimationFrame &frame : vertexAnimationFrames)
        {
            simple_smd_impl::AppendLine(output, "time " + std::to_string(frame.time));
            for (const VertexAnimationSample &sample : frame.samples)
            {
                std::ostringstream sampleLine;
                sampleLine << "  " << sample.vertexIndex << ' '
                           << sample.px << ' ' << sample.py << ' ' << sample.pz << ' '
                           << sample.nx << ' ' << sample.ny << ' ' << sample.nz;
                simple_smd_impl::AppendLine(output, sampleLine.str());
            }
        }
        simple_smd_impl::AppendLine(output, "end");
    }

    return output.str();
}
}
