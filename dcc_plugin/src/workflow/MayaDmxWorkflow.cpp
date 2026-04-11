#include "MayaDmxWorkflow.h"

#include "BatchManifestStore.h"
#include "WorkflowExecutor.h"
#include "WorkflowPresetStore.h"

namespace maya_dmx
{
MString SerializePreset(const ExportPreset &preset)
{
    WorkflowPresetStore store;
    return store.SerializePreset(preset);
}

bool DeserializePreset(const MString &text, ExportPreset &preset)
{
    WorkflowPresetStore store;
    return store.DeserializePreset(text, preset);
}

MString BuildTranslatorOptions(const ExportPreset &preset)
{
    WorkflowExecutor executor;
    return executor.BuildTranslatorOptions(preset);
}

bool ParseBatchManifestEntry(const MString &text, BatchManifestEntry &entry)
{
    BatchManifestStore store;
    return store.ParseBatchManifestEntry(text, entry);
}

MStatus SavePreset(const ExportPreset &preset)
{
    WorkflowPresetStore store;
    return store.SavePreset(preset);
}

MStatus LoadPreset(const MString &name, ExportPreset &preset)
{
    WorkflowPresetStore store;
    return store.LoadPreset(name, preset);
}

MStatus DeletePreset(const MString &name)
{
    WorkflowPresetStore store;
    return store.DeletePreset(name);
}

MStatus ListPresetNames(MStringArray &names)
{
    WorkflowPresetStore store;
    return store.ListPresetNames(names);
}

MStatus SaveBatchManifest(const MString &name, const MStringArray &entries)
{
    BatchManifestStore store;
    return store.SaveBatchManifest(name, entries);
}

MStatus LoadBatchManifest(const MString &name, MStringArray &entries)
{
    BatchManifestStore store;
    return store.LoadBatchManifest(name, entries);
}

MStatus DeleteBatchManifest(const MString &name)
{
    BatchManifestStore store;
    return store.DeleteBatchManifest(name);
}

MStatus ListBatchManifestNames(MStringArray &names)
{
    BatchManifestStore store;
    return store.ListBatchManifestNames(names);
}

MStatus ListLegacyBatchManifestNames(MStringArray &names)
{
    BatchManifestStore store;
    return store.ListLegacyBatchManifestNames(names);
}

MStatus MigrateLegacyBatchManifests(MStringArray &migratedNames)
{
    BatchManifestStore store;
    return store.MigrateLegacyBatchManifests(migratedNames);
}

MStatus CleanupBatchManifestStorage(MStringArray &removedItems)
{
    BatchManifestStore store;
    return store.CleanupBatchManifestStorage(removedItems);
}

MStatus ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection)
{
    WorkflowExecutor executor;
    return executor.ExecuteExport(preset, outputPath, exportSelection);
}

MStatus ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries)
{
    WorkflowExecutor executor;
    return executor.ExecuteBatchExport(preset, entries);
}
}
