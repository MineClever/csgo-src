#include "WorkflowPresetStore.h"

#include <common_dmx/MayaDmxCommon.h>
#include "WorkflowSupport.h"

#include <sstream>

namespace maya_dmx
{
namespace
{
MString FormatPresetDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str().c_str();
}
}

MString WorkflowPresetStore::SerializePreset(const ExportPreset &preset) const
{
    MString serialized;
    serialized += "outputDirectory=" + workflow_support::EscapeValue(preset.outputDirectory);
    serialized += ";materialRoot=" + workflow_support::EscapeValue(preset.materialRoot);
    serialized += ";dmxEncoding=" + workflow_support::EscapeValue(preset.dmxEncoding);
    serialized += ";upAxis=" + workflow_support::EscapeValue(preset.upAxis);
    serialized += ";translateX=" + FormatPresetDouble(preset.translateX);
    serialized += ";translateY=" + FormatPresetDouble(preset.translateY);
    serialized += ";translateZ=" + FormatPresetDouble(preset.translateZ);
    serialized += ";rotateX=" + FormatPresetDouble(preset.rotateX);
    serialized += ";rotateY=" + FormatPresetDouble(preset.rotateY);
    serialized += ";rotateZ=" + FormatPresetDouble(preset.rotateZ);
    serialized += ";scaleX=" + FormatPresetDouble(preset.scaleX);
    serialized += ";scaleY=" + FormatPresetDouble(preset.scaleY);
    serialized += ";scaleZ=" + FormatPresetDouble(preset.scaleZ);
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
        else if (key == "translateX")
        {
            preset.translateX = value.asDouble();
        }
        else if (key == "translateY")
        {
            preset.translateY = value.asDouble();
        }
        else if (key == "translateZ")
        {
            preset.translateZ = value.asDouble();
        }
        else if (key == "rotateX")
        {
            preset.rotateX = value.asDouble();
        }
        else if (key == "rotateY")
        {
            preset.rotateY = value.asDouble();
        }
        else if (key == "rotateZ")
        {
            preset.rotateZ = value.asDouble();
        }
        else if (key == "scaleX")
        {
            preset.scaleX = value.asDouble();
        }
        else if (key == "scaleY")
        {
            preset.scaleY = value.asDouble();
        }
        else if (key == "scaleZ")
        {
            preset.scaleZ = value.asDouble();
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
