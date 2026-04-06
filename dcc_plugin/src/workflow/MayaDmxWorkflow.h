#pragma once

#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

namespace maya_dmx
{
struct ExportPreset
{
    MString name;
    MString outputDirectory;
    MString materialRoot;
    MString dmxEncoding = "text";
    MString upAxis = "Y";
    bool exportSkin = true;
    bool exportDeltaStates = true;
    bool exportMetadata = true;
};

struct BatchManifestEntry
{
    MString rootPath;
    MString outputPath;
};

MString SerializePreset(const ExportPreset &preset);
bool DeserializePreset(const MString &text, ExportPreset &preset);
MString BuildTranslatorOptions(const ExportPreset &preset);
bool ParseBatchManifestEntry(const MString &text, BatchManifestEntry &entry);

MStatus SavePreset(const ExportPreset &preset);
MStatus LoadPreset(const MString &name, ExportPreset &preset);
MStatus DeletePreset(const MString &name);
MStatus ListPresetNames(MStringArray &names);

MStatus SaveBatchManifest(const MString &name, const MStringArray &entries);
MStatus LoadBatchManifest(const MString &name, MStringArray &entries);
MStatus DeleteBatchManifest(const MString &name);
MStatus ListBatchManifestNames(MStringArray &names);
MStatus ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection);
MStatus ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries);
}
