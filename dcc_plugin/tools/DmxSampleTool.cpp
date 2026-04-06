#include "../src/common/SimpleDmxText.h"
#include "../src/common/SimpleDmxWrite.h"

#include <fstream>
#include <iostream>
#include <string>

namespace
{
std::string ReadFile(const char *path)
{
    std::ifstream input(path, std::ios::in | std::ios::binary);
    std::string contents;
    input.seekg(0, std::ios::end);
    contents.resize(static_cast<size_t>(input.tellg()));
    input.seekg(0, std::ios::beg);
    if (!contents.empty())
    {
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    }
    return contents;
}

bool WriteFile(const char *path, const std::string &contents)
{
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

bool EndsWith(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: maya_dmx_sample_tool <input.dmx|input.dmxb> <output.dmx|output.dmxb>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    const std::string inputBytes = ReadFile(argv[1]);
    if (inputBytes.empty())
    {
        std::cerr << "failed to read input file: " << inputPath << "\n";
        return 1;
    }

    simple_dmx::Document document;
    std::string errorMessage;
    if (!simple_dmx::ParseDocument(inputBytes, document, errorMessage))
    {
        std::cerr << errorMessage << "\n";
        return 1;
    }

    std::string outputBytes;
    if (EndsWith(outputPath, ".dmxb") || EndsWith(outputPath, ".dmxbin"))
    {
        if (!simple_dmx::SerializeDocumentBinary(document, outputBytes, errorMessage))
        {
            std::cerr << errorMessage << "\n";
            return 1;
        }
    }
    else
    {
        outputBytes = simple_dmx::SerializeDocumentText(document);
    }

    if (!WriteFile(argv[2], outputBytes))
    {
        std::cerr << "failed to write output file: " << outputPath << "\n";
        return 1;
    }

    return 0;
}
