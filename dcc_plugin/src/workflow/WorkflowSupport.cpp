#include "WorkflowSupport.h"

#include <common/MayaCommandUtils.h>
#include <common_dmx/MayaDmxCommon.h>

#include <maya/MGlobal.h>

#include <algorithm>

namespace
{
void AppendChar(MString &text, char ch)
{
    const char charText[] = {ch, '\0'};
    text += charText;
}
}

namespace maya_dmx
{
namespace workflow_support
{
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
    const MStatus status = maya_cmd::GetMayaUserPrefDirectory(userPrefDir);
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
        return MStatus::kFailure;
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
    const MStatus status = maya_cmd::ListOptionVarNames(optionVarNames);
    if (!status)
    {
        return MStatus::kFailure;
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
}
