#include "WorkflowPresetStore.h"

#include <common/MayaDmxCommon.h>
#include "WorkflowSupport.h"

namespace maya_dmx
{
MString WorkflowPresetStore::SerializePreset(const ExportPreset &preset) const
{
    MString serialized;
    serialized += "outputDirectory=" + workflow_support::EscapeValue(preset.outputDirectory);
    serialized += ";materialRoot=" + workflow_support::EscapeValue(preset.materialRoot);
    serialized += ";dmxEncoding=" + workflow_support::EscapeValue(preset.dmxEncoding);
    serialized += ";upAxis=" + workflow_support::EscapeValue(preset.upAxis);
    serialized += ";exportSkin=";
    serialized += preset.exportSkin ? "1" : "0";
    serialized += ";exportDeltaStates=";
    serialized += preset.exportDeltaStates ? "1" : "0";
    serialized += ";exportMetadata=";
    serialized += preset.exportMetadata ? "1" : "0";
    return serialized;
}

bool WorkflowPresetStore::DeserializePreset(const MString &text, ExportPreset &preset) const
{
    const std::vector<MString> pairs = workflow_support::SplitEscaped(text, ';');
    for (const MString &pair : pairs)
    {
        if (pair.length() == 0)
        {
            continue;
        }

        const std::vector<MString> keyValue = workflow_support::SplitEscaped(pair, '=');
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
        value = workflow_support::UnescapeValue(value);

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
        else if (key == "exportMetadata")
        {
            preset.exportMetadata = (value == "1" || value == "true");
        }
    }

    return true;
}

MStatus WorkflowPresetStore::SavePreset(const ExportPreset &preset) const
{
    if (preset.name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    return workflow_support::SetOptionVarString(
        workflow_support::MakeOptionVarName(workflow_support::kPresetVarPrefix, preset.name),
        SerializePreset(preset));
}

MStatus WorkflowPresetStore::LoadPreset(const MString &name, ExportPreset &preset) const
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    MString serialized;
    if (!workflow_support::GetOptionVarString(
            workflow_support::MakeOptionVarName(workflow_support::kPresetVarPrefix, name),
            serialized))
    {
        return ReportError(MString("maya_dmx: export preset not found: ") + name);
    }

    preset = ExportPreset();
    preset.name = name;
    DeserializePreset(serialized, preset);
    return MS::kSuccess;
}

MStatus WorkflowPresetStore::DeletePreset(const MString &name) const
{
    if (name.length() == 0)
    {
        return ReportError("maya_dmx: preset name is required.");
    }

    return workflow_support::RemoveOptionVar(
        workflow_support::MakeOptionVarName(workflow_support::kPresetVarPrefix, name));
}

MStatus WorkflowPresetStore::ListPresetNames(MStringArray &names) const
{
    return workflow_support::CollectOptionVars(workflow_support::kPresetVarPrefix, names);
}
}
