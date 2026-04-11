#pragma once

#include "MayaDmxWorkflow.h"

namespace maya_dmx
{
class WorkflowExecutor
{
public:
    MString BuildTranslatorOptions(const ExportPreset &preset) const;
    MStatus ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection) const;
    MStatus ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries) const;
};
}
