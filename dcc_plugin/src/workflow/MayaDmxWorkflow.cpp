#include "MayaDmxWorkflow.h"

#include "../common/MayaDmxCommon.h"

#include <maya/MFileIO.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
constexpr const char *kPresetVarPrefix = "maya_dmx_preset_";
constexpr const char *kBatchVarPrefix = "maya_dmx_batch_";
constexpr const char *kBatchManifestDirectoryName = "maya_dmx_workflow";
constexpr const char *kBatchManifestExtension = ".batch";

void AppendChar(MString &text, char ch)
{
    const char charText[] = {ch, '\0'};
    text += charText;
}

MString EscapeValue(const MString &value)
{
    MString escaped = value;
    escaped.substitute("\\", "\\\\");
    escaped.substitute(";", "\\;");
    escaped.substitute("=", "\\=");
    escaped.substitute("|", "\\|");
    escaped.substitute("\n", "\\n");
    return escaped;
}

MString UnescapeValue(const MString &value)
{
    MString result;
    const char *chars = value.asChar();
    for (size_t i = 0; chars[i] != '\0'; ++i)
    {
        if (chars[i] == '\\' && chars[i + 1] != '\0')
        {
            ++i;
            switch (chars[i])
            {
            case 'n':
                result += "\n";
                break;
            default:
                AppendChar(result, chars[i]);
                break;
            }
        }
        else
        {
            AppendChar(result, chars[i]);
        }
    }
    return result;
}

MString EncodeHexString(const MString &value)
{
    static const char *kHexDigits = "0123456789ABCDEF";
    MString encoded;
    const char *chars = value.asChar();
    for (size_t i = 0; chars[i] != '\0'; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(chars[i]);
        const char hexPair[] = {
            kHexDigits[(ch >> 4) & 0x0F],
            kHexDigits[ch & 0x0F],
            '\0'};
        encoded += hexPair;
    }
    return encoded;
}

MString EncodeHexBytes(const std::string &value)
{
    static const char *kHexDigits = "0123456789ABCDEF";
    MString encoded;
    for (unsigned char ch : value)
    {
        const char hexPair[] = {
            kHexDigits[(ch >> 4) & 0x0F],
            kHexDigits[ch & 0x0F],
            '\0'};
        encoded += hexPair;
    }
    return encoded;
}

bool DecodeHexString(const MString &value, MString &decoded)
{
    decoded = "";
    const char *chars = value.asChar();
    size_t length = value.length();
    if ((length % 2) != 0)
    {
        return false;
    }

    const auto decodeNibble = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        return -1;
    };

    for (size_t index = 0; index < length; index += 2)
    {
        const int hi = decodeNibble(chars[index]);
        const int lo = decodeNibble(chars[index + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }

        AppendChar(decoded, static_cast<char>((hi << 4) | lo));
    }

    return true;
}

bool DecodeHexBytes(const std::string &value, std::string &decoded)
{
    decoded.clear();
    if ((value.length() % 2) != 0)
    {
        return false;
    }

    const auto decodeNibble = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        return -1;
    };

    for (size_t index = 0; index < value.length(); index += 2)
    {
        const int hi = decodeNibble(value[index]);
        const int lo = decodeNibble(value[index + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }

        decoded.push_back(static_cast<char>((hi << 4) | lo));
    }

    return true;
}

std::vector<MString> SplitEscaped(const MString &text, char delimiter)
{
    std::vector<MString> parts;
    MString current;
    const char *chars = text.asChar();
    bool escaped = false;
    for (size_t i = 0; chars[i] != '\0'; ++i)
    {
        const char ch = chars[i];
        if (escaped)
        {
            AppendChar(current, '\\');
            AppendChar(current, ch);
            escaped = false;
            continue;
        }
        if (ch == '\\')
        {
            escaped = true;
            continue;
        }
        if (ch == delimiter)
        {
            parts.push_back(current);
            current = "";
            continue;
        }
        AppendChar(current, ch);
    }
    if (escaped)
    {
        AppendChar(current, '\\');
    }
    parts.push_back(current);
    return parts;
}

MString MakeOptionVarName(const char *prefix, const MString &name)
{
    return MString(prefix) + name;
}

MStatus GetMayaUserPrefDirectory(std::filesystem::path &directory)
{
    MString userPrefDir;
    const MStatus status = MGlobal::executeCommand("internalVar -userPrefDir", userPrefDir);
    if (!status || userPrefDir.length() == 0)
    {
        return maya_dmx::ReportError("maya_dmx: failed to resolve Maya user preference directory.", status);
    }

    directory = std::filesystem::path(userPrefDir.asChar());
    return MS::kSuccess;
}

MStatus GetBatchManifestDirectory(std::filesystem::path &directory)
{
    std::filesystem::path userPrefDirectory;
    MStatus status = GetMayaUserPrefDirectory(userPrefDirectory);
    if (!status)
    {
        return status;
    }

    directory = userPrefDirectory / kBatchManifestDirectoryName;

    std::error_code errorCode;
    std::filesystem::create_directories(directory, errorCode);
    if (errorCode)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to create workflow directory ") + directory.generic_string().c_str());
    }

    return MS::kSuccess;
}

std::filesystem::path GetBatchManifestPath(const MString &name)
{
    std::filesystem::path directory;
    if (!GetBatchManifestDirectory(directory))
    {
        return {};
    }

    const MString encodedName = EncodeHexString(name);
    return directory / (std::string(encodedName.asChar()) + kBatchManifestExtension);
}

MString JoinPath(const MString &baseDirectory, const MString &relativePath)
{
    std::filesystem::path path = baseDirectory.length() == 0 ?
        std::filesystem::path(relativePath.asChar()) :
        (std::filesystem::path(baseDirectory.asChar()) / relativePath.asChar());
    return path.lexically_normal().generic_string().c_str();
}

MStatus EnsureParentDirectory(const MString &path)
{
    const std::filesystem::path filesystemPath(path.asChar());
    const std::filesystem::path parentPath = filesystemPath.parent_path();
    if (parentPath.empty())
    {
        return MS::kSuccess;
    }

    std::error_code errorCode;
    std::filesystem::create_directories(parentPath, errorCode);
    if (errorCode)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to create output directory ") + parentPath.generic_string().c_str());
    }

    return MS::kSuccess;
}

MStatus SetOptionVarString(const MString &name, const MString &value)
{
    return MGlobal::setOptionVarValue(name, value) ? MS::kSuccess : MS::kFailure;
}

bool GetOptionVarString(const MString &name, MString &value)
{
    if (!MGlobal::optionVarExists(name))
    {
        return false;
    }

    value = MGlobal::optionVarStringValue(name);
    return true;
}

MStatus RemoveOptionVar(const MString &name)
{
    if (!MGlobal::optionVarExists(name))
    {
        return MS::kSuccess;
    }
    MGlobal::removeOptionVar(name);
    return MS::kSuccess;
}

MStatus CollectOptionVars(const char *prefix, MStringArray &names)
{
    names.clear();

    MStringArray optionVarNames;
    const MStatus status = MGlobal::executeCommand("optionVar -list", optionVarNames);
    if (!status)
    {
        return status;
    }

    const MString prefixString(prefix);
    for (unsigned int i = 0; i < optionVarNames.length(); ++i)
    {
        const MString &optionVarName = optionVarNames[i];
        if (optionVarName.indexW(prefixString) == 0)
        {
            names.append(optionVarName.substringW(prefixString.length(), optionVarName.length() - 1));
        }
    }

    std::vector<MString> sorted;
    sorted.reserve(names.length());
    for (unsigned int i = 0; i < names.length(); ++i)
    {
        sorted.push_back(names[i]);
    }
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const MString &lhs, const MString &rhs)
        {
            return lhs.asChar() < rhs.asChar();
        });

    names.clear();
    for (const MString &name : sorted)
    {
        names.append(name);
    }

    return MS::kSuccess;
}
}

namespace maya_dmx
{
MString SerializePreset(const ExportPreset &preset)
{
    MString serialized;
    serialized += "outputDirectory=" + EscapeValue(preset.outputDirectory);
    serialized += ";materialRoot=" + EscapeValue(preset.materialRoot);
    serialized += ";dmxEncoding=" + EscapeValue(preset.dmxEncoding);
    serialized += ";upAxis=" + EscapeValue(preset.upAxis);
    serialized += ";exportSkin=";
    serialized += preset.exportSkin ? "1" : "0";
    serialized += ";exportDeltaStates=";
    serialized += preset.exportDeltaStates ? "1" : "0";
    return serialized;
}

bool DeserializePreset(const MString &text, ExportPreset &preset)
{
    const std::vector<MString> pairs = SplitEscaped(text, ';');
    for (const MString &pair : pairs)
    {
        if (pair.length() == 0)
        {
            continue;
        }

        const std::vector<MString> keyValue = SplitEscaped(pair, '=');
        if (keyValue.size() < 2)
        {
            continue;
        }

        const MString &key = keyValue[0];
        MString value;
        for (size_t i = 1; i < keyValue.size(); ++i)
        {
            if (i > 1)
            {
                value += "=";
            }
            value += keyValue[i];
        }
        value = UnescapeValue(value);

        if (key == "outputDirectory")
        {
            preset.outputDirectory = value;
        }
        else if (key == "materialRoot")
        {
            preset.materialRoot = value;
        }
        else if (key == "dmxEncoding")
        {
            preset.dmxEncoding = value;
        }
        else if (key == "upAxis")
        {
            preset.upAxis = value;
        }
        else if (key == "exportSkin")
        {
            preset.exportSkin = (value == "1" || value == "true");
        }
        else if (key == "exportDeltaStates")
        {
            preset.exportDeltaStates = (value == "1" || value == "true");
        }
    }

    return true;
}

MString BuildTranslatorOptions(const ExportPreset &preset)
{
    MString options;
    options += "encoding=";
    options += preset.dmxEncoding.length() == 0 ? "text" : preset.dmxEncoding;
    options += ";upAxis=";
    options += preset.upAxis.length() == 0 ? "Y" : preset.upAxis;
    options += ";exportSkin=";
    options += preset.exportSkin ? "1" : "0";
    options += ";exportDeltaStates=";
    options += preset.exportDeltaStates ? "1" : "0";
    if (preset.materialRoot.length() > 0)
    {
        options += ";materialRoot=";
        options += preset.materialRoot;
    }
    return options;
}

bool ParseBatchManifestEntry(const MString &text, BatchManifestEntry &entry)
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

    entry.rootPath = UnescapeValue(text.substringW(0, splitIndex - 1));
    entry.outputPath = UnescapeValue(text.substringW(splitIndex + 1, text.length() - 1));
    return entry.rootPath.length() > 0 && entry.outputPath.length() > 0;
}

MStatus SavePreset(const ExportPreset &preset)
{
    if (preset.name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    return SetOptionVarString(MakeOptionVarName(kPresetVarPrefix, preset.name), SerializePreset(preset));
}

MStatus LoadPreset(const MString &name, ExportPreset &preset)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    MString serialized;
    if (!GetOptionVarString(MakeOptionVarName(kPresetVarPrefix, name), serialized))
    {
        return ReportError(MString("maya_dmx: export preset not found: ") + name);
    }

    preset = ExportPreset();
    preset.name = name;
    DeserializePreset(serialized, preset);
    return MS::kSuccess;
}

MStatus DeletePreset(const MString &name)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    return RemoveOptionVar(MakeOptionVarName(kPresetVarPrefix, name));
}

MStatus ListPresetNames(MStringArray &names)
{
    return CollectOptionVars(kPresetVarPrefix, names);
}

MStatus SaveBatchManifest(const MString &name, const MStringArray &entries)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    const std::filesystem::path manifestPath = GetBatchManifestPath(name);
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
        output << EncodeHexString(entries[i]).asChar();
    }

    output.close();
    if (!output)
    {
        return ReportError(MString("maya_dmx: failed to write batch manifest: ") + manifestPath.generic_string().c_str());
    }

    return RemoveOptionVar(MakeOptionVarName(kBatchVarPrefix, name));
}

MStatus LoadBatchManifest(const MString &name, MStringArray &entries)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    entries.clear();
    const std::filesystem::path manifestPath = GetBatchManifestPath(name);
    if (!manifestPath.empty() && std::filesystem::exists(manifestPath))
    {
        std::ifstream input(manifestPath, std::ios::binary);
        if (!input)
        {
            return ReportError(MString("maya_dmx: failed to open batch manifest: ") + manifestPath.generic_string().c_str());
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }

            MString decoded;
            if (!DecodeHexString(line.c_str(), decoded))
            {
                return ReportError(MString("maya_dmx: batch manifest entry was corrupted: ") + line.c_str());
            }
            entries.append(decoded);
        }

        if (!input.eof() && input.fail())
        {
            return ReportError(MString("maya_dmx: failed to read batch manifest: ") + manifestPath.generic_string().c_str());
        }

        return MS::kSuccess;
    }

    // One-time fallback for older optionVar-backed manifests.
    MString serialized;
    if (!GetOptionVarString(MakeOptionVarName(kBatchVarPrefix, name), serialized))
    {
        return ReportError(MString("maya_dmx: batch manifest not found: ") + name);
    }

    const std::vector<MString> parts = SplitEscaped(serialized, '\n');
    for (const MString &part : parts)
    {
        if (part.length() > 0)
        {
            MString decoded;
            if (!DecodeHexString(part, decoded))
            {
                return ReportError(MString("maya_dmx: batch manifest entry was corrupted: ") + part);
            }
            entries.append(decoded);
        }
    }
    return MS::kSuccess;
}

MStatus DeleteBatchManifest(const MString &name)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    const std::filesystem::path manifestPath = GetBatchManifestPath(name);
    if (!manifestPath.empty())
    {
        std::error_code errorCode;
        std::filesystem::remove(manifestPath, errorCode);
        if (errorCode)
        {
            return ReportError(MString("maya_dmx: failed to delete batch manifest: ") + manifestPath.generic_string().c_str());
        }
    }

    return RemoveOptionVar(MakeOptionVarName(kBatchVarPrefix, name));
}

MStatus ListBatchManifestNames(MStringArray &names)
{
    names.clear();

    std::filesystem::path directory;
    MStatus status = GetBatchManifestDirectory(directory);
    if (!status)
    {
        return status;
    }

    std::vector<MString> sorted;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != kBatchManifestExtension)
        {
            continue;
        }

        std::string decodedName;
        if (!DecodeHexBytes(entry.path().stem().string(), decodedName))
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

MStatus ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection)
{
    if (outputPath.length() == 0)
    {
        return ReportError("maya_dmx: output path is required.");
    }

    const MString normalizedOutputPath = std::filesystem::path(outputPath.asChar()).lexically_normal().generic_string().c_str();

    MStatus status = EnsureParentDirectory(normalizedOutputPath);
    if (!status)
    {
        return status;
    }

    {
        std::error_code errorCode;
        std::filesystem::remove(std::filesystem::path(normalizedOutputPath.asChar()), errorCode);
    }

    MString previousOptions;
    const bool hadPreviousOptions = MGlobal::optionVarExists("FileTranslatorOptions");
    if (hadPreviousOptions)
    {
        previousOptions = MGlobal::optionVarStringValue("FileTranslatorOptions");
    }

    MGlobal::setOptionVarValue("FileTranslatorOptions", BuildTranslatorOptions(preset));
    status = exportSelection ?
        MFileIO::exportSelected(normalizedOutputPath, "Valve DMX Export", false) :
        MFileIO::exportAll(normalizedOutputPath, "Valve DMX Export", false);

    if (hadPreviousOptions)
    {
        MGlobal::setOptionVarValue("FileTranslatorOptions", previousOptions);
    }
    else
    {
        MGlobal::removeOptionVar("FileTranslatorOptions");
    }

    if (!status)
    {
        return ReportError(MString("maya_dmx: workflow export failed for ") + normalizedOutputPath, status);
    }

    return MS::kSuccess;
}

MStatus ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries)
{
    if (entries.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest has no entries.");
    }

    MSelectionList originalSelection;
    MGlobal::getActiveSelectionList(originalSelection);

    for (unsigned int entryIndex = 0; entryIndex < entries.length(); ++entryIndex)
    {
        BatchManifestEntry entry;
        if (!ParseBatchManifestEntry(entries[entryIndex], entry))
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return ReportError(MString("maya_dmx: invalid batch entry: ") + entries[entryIndex]);
        }

        MSelectionList exportSelection;
        MStatus status = exportSelection.add(entry.rootPath);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return ReportError(MString("maya_dmx: batch root was not found: ") + entry.rootPath, status);
        }

        status = MGlobal::setActiveSelectionList(exportSelection);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return status;
        }

        status = ExecuteExport(preset, JoinPath(preset.outputDirectory, entry.outputPath), true);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return status;
        }
    }

    MGlobal::setActiveSelectionList(originalSelection);
    return MS::kSuccess;
}
}
