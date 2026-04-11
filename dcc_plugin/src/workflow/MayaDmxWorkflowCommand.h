#pragma once

#include "MayaDmxWorkflow.h"

#include <memory>

#include <maya/MArgDatabase.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class MayaDmxWorkflowCommand : public MPxCommand
{
public:
    static void *Create();
    static MSyntax CreateSyntax();

    MStatus doIt(const MArgList &args) override;
    bool isUndoable() const override;

private:
    static constexpr const char *kCommandName = "mayaDmxWorkflow";
    static constexpr const char *kSavePresetFlag = "-sp";
    static constexpr const char *kSavePresetLongFlag = "-savePreset";
    static constexpr const char *kLoadPresetFlag = "-lp";
    static constexpr const char *kLoadPresetLongFlag = "-loadPreset";
    static constexpr const char *kDeletePresetFlag = "-dp";
    static constexpr const char *kDeletePresetLongFlag = "-deletePreset";
    static constexpr const char *kListPresetsFlag = "-ls";
    static constexpr const char *kListPresetsLongFlag = "-listPresets";
    static constexpr const char *kOutputDirectoryFlag = "-od";
    static constexpr const char *kOutputDirectoryLongFlag = "-outputDirectory";
    static constexpr const char *kMaterialRootFlag = "-mr";
    static constexpr const char *kMaterialRootLongFlag = "-materialRoot";
    static constexpr const char *kEncodingFlag = "-enc";
    static constexpr const char *kEncodingLongFlag = "-encoding";
    static constexpr const char *kUpAxisFlag = "-ua";
    static constexpr const char *kUpAxisLongFlag = "-upAxis";
    static constexpr const char *kExportSkinFlag = "-es";
    static constexpr const char *kExportSkinLongFlag = "-exportSkin";
    static constexpr const char *kExportDeltaFlag = "-eds";
    static constexpr const char *kExportDeltaLongFlag = "-exportDeltaStates";
    static constexpr const char *kExportMetadataFlag = "-emd";
    static constexpr const char *kExportMetadataLongFlag = "-exportMetadata";
    static constexpr const char *kSaveBatchFlag = "-sb";
    static constexpr const char *kSaveBatchLongFlag = "-saveBatch";
    static constexpr const char *kLoadBatchFlag = "-lb";
    static constexpr const char *kLoadBatchLongFlag = "-loadBatch";
    static constexpr const char *kDeleteBatchFlag = "-db";
    static constexpr const char *kDeleteBatchLongFlag = "-deleteBatch";
    static constexpr const char *kListBatchesFlag = "-lbt";
    static constexpr const char *kListBatchesLongFlag = "-listBatches";
    static constexpr const char *kListLegacyBatchesFlag = "-llb";
    static constexpr const char *kListLegacyBatchesLongFlag = "-listLegacyBatches";
    static constexpr const char *kBatchEntryFlag = "-be";
    static constexpr const char *kBatchEntryLongFlag = "-batchEntry";
    static constexpr const char *kMigrateLegacyBatchesFlag = "-mlb";
    static constexpr const char *kMigrateLegacyBatchesLongFlag = "-migrateLegacyBatches";
    static constexpr const char *kCleanupBatchStorageFlag = "-cbt";
    static constexpr const char *kCleanupBatchStorageLongFlag = "-cleanupBatchStorage";
    static constexpr const char *kExportPresetFlag = "-ep";
    static constexpr const char *kExportPresetLongFlag = "-exportPreset";
    static constexpr const char *kExportPathFlag = "-fp";
    static constexpr const char *kExportPathLongFlag = "-filePath";
    static constexpr const char *kExportAllFlag = "-ea";
    static constexpr const char *kExportAllLongFlag = "-exportAll";
    static constexpr const char *kRunBatchFlag = "-rb";
    static constexpr const char *kRunBatchLongFlag = "-runBatch";

    void resetState();
    MArgDatabase &arguments();
    const MArgDatabase &arguments() const;
    bool isFlagSet(const char *flagName) const;
    void appendJoinedResult(const MStringArray &items, MString &result) const;
    MStatus setJoinedResult(const MStringArray &items);
    MStatus executeListCommand(MStatus (*operation)(MStringArray &));
    void populatePresetFromArgs(const char *nameFlag);
    void collectBatchEntries();
    MStatus loadPresetArgument(const char *flagName);
    MStatus handleLoadPreset();
    MStatus handleSavePreset();
    MStatus handleLoadBatch();
    MStatus handleSaveBatch();
    MStatus handleRunBatch();
    MStatus handleExportPreset();

    std::unique_ptr<MArgDatabase> arguments_;
    maya_dmx::ExportPreset workingPreset_;
    MStringArray workingEntries_;
    MString workingName_;
    MString workingOutputPath_;
    bool workingExportAll_ = false;
};
