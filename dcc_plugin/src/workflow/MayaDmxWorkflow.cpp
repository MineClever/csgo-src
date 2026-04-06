#include "MayaDmxWorkflow.h"

#include "../common/MayaDmxCommon.h"

#include <maya/MGlobal.h>

#include <algorithm>
#include <vector>

namespace
{
constexpr const char *kPresetVarPrefix = "maya_dmx_preset_";
constexpr const char *kBatchVarPrefix = "maya_dmx_batch_";

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
                result += chars[i];
                break;
            }
        }
        else
        {
            result += chars[i];
        }
    }
    return result;
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
            current += '\\';
            current += ch;
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
        current += ch;
    }
    if (escaped)
    {
        current += '\\';
    }
    parts.push_back(current);
    return parts;
}

MString MakeOptionVarName(const char *prefix, const MString &name)
{
    return MString(prefix) + name;
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

    MString serialized;
    for (unsigned int i = 0; i < entries.length(); ++i)
    {
        if (i > 0)
        {
            serialized += "\n";
        }
        serialized += EscapeValue(entries[i]);
    }

    return SetOptionVarString(MakeOptionVarName(kBatchVarPrefix, name), serialized);
}

MStatus LoadBatchManifest(const MString &name, MStringArray &entries)
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest name is required.");
    }

    MString serialized;
    if (!GetOptionVarString(MakeOptionVarName(kBatchVarPrefix, name), serialized))
    {
        return ReportError(MString("maya_dmx: batch manifest not found: ") + name);
    }

    entries.clear();
    const std::vector<MString> parts = SplitEscaped(serialized, '\n');
    for (const MString &part : parts)
    {
        if (part.length() > 0)
        {
            entries.append(UnescapeValue(part));
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

    return RemoveOptionVar(MakeOptionVarName(kBatchVarPrefix, name));
}

MStatus ListBatchManifestNames(MStringArray &names)
{
    return CollectOptionVars(kBatchVarPrefix, names);
}
}
