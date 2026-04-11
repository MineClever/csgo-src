#include "BatchManifestStore.h"

#include "../common/MayaDmxCommon.h"
#include "WorkflowSupport.h"

#include <maya/MGlobal.h>

#include <algorithm>
#include <fstream>

namespace maya_dmx
{
bool BatchManifestStore::ParseBatchManifestEntry(const MString &text, BatchManifestEntry &entry) const
{
    const char *chars = text.asChar();
    int splitIndex = -1;
    bool escaped = false;
    for (int index = 0; chars[index] != '\0'; ++index)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (chars[index] == '\\')
        {
            escaped = true;
            continue;
        }
        if (chars[index] == '|')
        {
            splitIndex = index;
        }
    }

    if (splitIndex <= 0 || chars[splitIndex + 1] == '\0')
    {
        return false;
    }

    entry.rootPath = workflow_support::UnescapeValue(text.substringW(0, splitIndex - 1));
    entry.outputPath = workflow_support::UnescapeValue(text.substringW(splitIndex + 1, text.length() - 1));
    return entry.rootPath.length() > 0 && entry.outputPath.length() > 0;
}

MStatus BatchManifestStore::LoadLegacyBatchManifest(const MString &name, MStringArray &entries) const
{
    entries.clear();

    MString serialized;
    if (!workflow_support::GetOptionVarString(
            workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name),
            serialized))
    {
        return ReportError(MString("maya_dmx: legacy batch manifest not found: ") + name);
    }

    const std::vector<MString> parts = workflow_support::SplitEscaped(serialized, '\n');
    for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
    {
        const MString &part = parts[partIndex];
        if (part.length() == 0)
        {
            continue;
        }

        MString decoded;
        if (!workflow_support::DecodeHexString(part, decoded))
        {
            return ReportError(
                MString("maya_dmx: legacy batch manifest entry was corrupted at line ")
                + static_cast<int>(partIndex + 1)
                + ": " + part);
        }

        entries.append(decoded);
    }

    return MS::kSuccess;
}

MStatus BatchManifestStore::ReadBatchManifestFile(const std::filesystem::path &manifestPath, MStringArray &entries) const
{
    entries.clear();

    std::ifstream input(manifestPath, std::ios::binary);
    if (!input)
    {
        return ReportError(MString("maya_dmx: failed to open batch manifest: ") + manifestPath.generic_string().c_str());
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (line.empty())
        {
            continue;
        }

        MString decoded;
        if (!workflow_support::DecodeHexString(line.c_str(), decoded))
        {
            return ReportError(
                MString("maya_dmx: batch manifest entry was corrupted at ")
                + manifestPath.generic_string().c_str()
                + ": line " + static_cast<int>(lineNumber)
                + " value=" + line.c_str());
        }

        entries.append(decoded);
    }

    if (!input.eof() && input.fail())
    {
        return ReportError(MString("maya_dmx: failed to read batch manifest: ") + manifestPath.generic_string().c_str());
    }

    return MS::kSuccess;
}

MStatus BatchManifestStore::SaveBatchManifest(const MString &name, const MStringArray &entries) const
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    const std::filesystem::path manifestPath = workflow_support::GetBatchManifestPath(name);
    if (manifestPath.empty())
    {
        return ReportError("maya_dmx: batch manifest path could not be resolved.");
    }

    std::ofstream output(manifestPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return ReportError(MString("maya_dmx: failed to open batch manifest for writing: ") + manifestPath.generic_string().c_str());
    }

    for (unsigned int i = 0; i < entries.length(); ++i)
    {
        if (i > 0)
        {
            output << '\n';
        }
        output << workflow_support::EncodeHexString(entries[i]).asChar();
    }

    output.close();
    if (!output)
    {
        return ReportError(MString("maya_dmx: failed to write batch manifest: ") + manifestPath.generic_string().c_str());
    }

    return workflow_support::RemoveOptionVar(
        workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name));
}

MStatus BatchManifestStore::LoadBatchManifest(const MString &name, MStringArray &entries) const
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    entries.clear();
    const std::filesystem::path manifestPath = workflow_support::GetBatchManifestPath(name);
    if (!manifestPath.empty() && std::filesystem::exists(manifestPath))
    {
        return ReadBatchManifestFile(manifestPath, entries);
    }

    if (!MGlobal::optionVarExists(workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name)))
    {
        return ReportError(MString("maya_dmx: batch manifest not found: ") + name);
    }

    return LoadLegacyBatchManifest(name, entries);
}

MStatus BatchManifestStore::DeleteBatchManifest(const MString &name) const
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    const std::filesystem::path manifestPath = workflow_support::GetBatchManifestPath(name);
    if (!manifestPath.empty())
    {
        std::error_code errorCode;
        std::filesystem::remove(manifestPath, errorCode);
        if (errorCode)
        {
            return ReportError(MString("maya_dmx: failed to delete batch manifest: ") + manifestPath.generic_string().c_str());
        }
    }

    return workflow_support::RemoveOptionVar(
        workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name));
}

MStatus BatchManifestStore::ListBatchManifestNames(MStringArray &names) const
{
    names.clear();

    std::filesystem::path directory;
    MStatus status = workflow_support::GetBatchManifestDirectory(directory);
    if (!status)
    {
        return MStatus::kFailure;
    }

    std::vector<MString> sorted;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != workflow_support::kBatchManifestExtension)
        {
            continue;
        }

        std::string decodedName;
        if (!workflow_support::DecodeHexBytes(entry.path().stem().string(), decodedName))
        {
            continue;
        }

        sorted.emplace_back(decodedName.c_str());
    }

    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const MString &lhs, const MString &rhs)
        {
            return lhs.asChar() < rhs.asChar();
        });

    for (const MString &name : sorted)
    {
        names.append(name);
    }

    return MS::kSuccess;
}

MStatus BatchManifestStore::ListLegacyBatchManifestNames(MStringArray &names) const
{
    return workflow_support::CollectOptionVars(workflow_support::kBatchVarPrefix, names);
}

MStatus BatchManifestStore::MigrateLegacyBatchManifests(MStringArray &migratedNames) const
{
    migratedNames.clear();

    MStringArray legacyNames;
    MStatus status = ListLegacyBatchManifestNames(legacyNames);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (unsigned int index = 0; index < legacyNames.length(); ++index)
    {
        const MString &name = legacyNames[index];
        const std::filesystem::path manifestPath = workflow_support::GetBatchManifestPath(name);
        if (manifestPath.empty())
        {
            return ReportError(MString("maya_dmx: batch manifest path could not be resolved for migration: ") + name);
        }

        if (std::filesystem::exists(manifestPath))
        {
            workflow_support::RemoveOptionVar(
                workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name));
            migratedNames.append(name + " (legacy optionVar removed; file already existed)");
            continue;
        }

        MStringArray entries;
        status = LoadLegacyBatchManifest(name, entries);
        if (!status)
        {
            return MStatus::kFailure;
        }

        status = SaveBatchManifest(name, entries);
        if (!status)
        {
            return MStatus::kFailure;
        }

        migratedNames.append(name);
    }

    return MS::kSuccess;
}

MStatus BatchManifestStore::CleanupBatchManifestStorage(MStringArray &removedItems) const
{
    removedItems.clear();

    std::filesystem::path directory;
    MStatus status = workflow_support::GetBatchManifestDirectory(directory);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != workflow_support::kBatchManifestExtension)
        {
            continue;
        }

        std::string decodedName;
        if (workflow_support::DecodeHexBytes(entry.path().stem().string(), decodedName))
        {
            continue;
        }

        std::error_code errorCode;
        std::filesystem::remove(entry.path(), errorCode);
        if (errorCode)
        {
            return ReportError(MString("maya_dmx: failed to delete invalid batch manifest: ") + entry.path().generic_string().c_str());
        }

        removedItems.append(MString("invalid_file:") + entry.path().generic_string().c_str());
    }

    MStringArray legacyNames;
    status = ListLegacyBatchManifestNames(legacyNames);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (unsigned int index = 0; index < legacyNames.length(); ++index)
    {
        const MString &name = legacyNames[index];
        const std::filesystem::path manifestPath = workflow_support::GetBatchManifestPath(name);
        if (!manifestPath.empty() && std::filesystem::exists(manifestPath))
        {
            status = workflow_support::RemoveOptionVar(
                workflow_support::MakeOptionVarName(workflow_support::kBatchVarPrefix, name));
            if (!status)
            {
                return MStatus::kFailure;
            }

            removedItems.append(MString("legacy_optionVar:") + name);
        }
    }

    return MS::kSuccess;
}
}
