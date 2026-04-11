#pragma once

#include "MayaDmxWorkflow.h"

namespace maya_dmx
{
class WorkflowPresetStore
{
public:
    MString SerializePreset(const ExportPreset &preset) const;
    bool DeserializePreset(const MString &text, ExportPreset &preset) const;

    MStatus SavePreset(const ExportPreset &preset) const;
    MStatus LoadPreset(const MString &name, ExportPreset &preset) const;
    MStatus DeletePreset(const MString &name) const;
    MStatus ListPresetNames(MStringArray &names) const;
};
}
