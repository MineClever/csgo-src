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
    double translateX = 0.0;
    double translateY = 0.0;
    double translateZ = 0.0;
    double rotateX = 0.0;
    double rotateY = 0.0;
    double rotateZ = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double scaleZ = 1.0;
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
MStatus ListLegacyBatchManifestNames(MStringArray &names);
MStatus MigrateLegacyBatchManifests(MStringArray &migratedNames);
MStatus CleanupBatchManifestStorage(MStringArray &removedItems);
MStatus ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection);
MStatus ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries);
}
