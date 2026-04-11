#include "MayaDmxWorkflowCommand.h"

#include <common/MayaDmxCommon.h>

#include <maya/MArgList.h>

void *MayaDmxWorkflowCommand::Create()
{
    return new MayaDmxWorkflowCommand();
}

MSyntax MayaDmxWorkflowCommand::CreateSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(false);
    syntax.enableEdit(false);

    syntax.addFlag(kSavePresetFlag, kSavePresetLongFlag, MSyntax::kString);
    syntax.addFlag(kLoadPresetFlag, kLoadPresetLongFlag, MSyntax::kString);
    syntax.addFlag(kDeletePresetFlag, kDeletePresetLongFlag, MSyntax::kString);
    syntax.addFlag(kListPresetsFlag, kListPresetsLongFlag);
    syntax.addFlag(kOutputDirectoryFlag, kOutputDirectoryLongFlag, MSyntax::kString);
    syntax.addFlag(kMaterialRootFlag, kMaterialRootLongFlag, MSyntax::kString);
    syntax.addFlag(kEncodingFlag, kEncodingLongFlag, MSyntax::kString);
    syntax.addFlag(kUpAxisFlag, kUpAxisLongFlag, MSyntax::kString);
    syntax.addFlag(kExportSkinFlag, kExportSkinLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kExportDeltaFlag, kExportDeltaLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kExportMetadataFlag, kExportMetadataLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kSaveBatchFlag, kSaveBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kLoadBatchFlag, kLoadBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kDeleteBatchFlag, kDeleteBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kListBatchesFlag, kListBatchesLongFlag);
    syntax.addFlag(kListLegacyBatchesFlag, kListLegacyBatchesLongFlag);
    syntax.addFlag(kBatchEntryFlag, kBatchEntryLongFlag, MSyntax::kString);
    syntax.addFlag(kMigrateLegacyBatchesFlag, kMigrateLegacyBatchesLongFlag);
    syntax.addFlag(kCleanupBatchStorageFlag, kCleanupBatchStorageLongFlag);
    syntax.addFlag(kExportPresetFlag, kExportPresetLongFlag, MSyntax::kString);
    syntax.addFlag(kExportPathFlag, kExportPathLongFlag, MSyntax::kString);
    syntax.addFlag(kExportAllFlag, kExportAllLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kRunBatchFlag, kRunBatchLongFlag, MSyntax::kString);
    syntax.makeFlagMultiUse(kBatchEntryFlag);
    return syntax;
}

MStatus MayaDmxWorkflowCommand::doIt(const MArgList &args)
{
    arguments_ = std::make_unique<MArgDatabase>(syntax(), args);
    resetState();

    if (isFlagSet(kListPresetsFlag))
    {
        return executeListCommand(maya_dmx::ListPresetNames);
    }

    if (isFlagSet(kListBatchesFlag))
    {
        return executeListCommand(maya_dmx::ListBatchManifestNames);
    }

    if (isFlagSet(kListLegacyBatchesFlag))
    {
        return executeListCommand(maya_dmx::ListLegacyBatchManifestNames);
    }

    if (isFlagSet(kMigrateLegacyBatchesFlag))
    {
        return executeListCommand(maya_dmx::MigrateLegacyBatchManifests);
    }

    if (isFlagSet(kCleanupBatchStorageFlag))
    {
        return executeListCommand(maya_dmx::CleanupBatchManifestStorage);
    }

    if (isFlagSet(kLoadPresetFlag))
    {
        return handleLoadPreset();
    }

    if (isFlagSet(kDeletePresetFlag))
    {
        arguments().getFlagArgument(kDeletePresetFlag, 0, workingName_);
        return maya_dmx::DeletePreset(workingName_);
    }

    if (isFlagSet(kSavePresetFlag))
    {
        return handleSavePreset();
    }

    if (isFlagSet(kLoadBatchFlag))
    {
        return handleLoadBatch();
    }

    if (isFlagSet(kDeleteBatchFlag))
    {
        arguments().getFlagArgument(kDeleteBatchFlag, 0, workingName_);
        return maya_dmx::DeleteBatchManifest(workingName_);
    }

    if (isFlagSet(kSaveBatchFlag))
    {
        return handleSaveBatch();
    }

    if (isFlagSet(kRunBatchFlag))
    {
        return handleRunBatch();
    }

    if (isFlagSet(kExportPresetFlag))
    {
        return handleExportPreset();
    }

    return maya_dmx::ReportError(
        MString("maya_dmx: no workflow action specified. Use ") + kCommandName +
        " with -savePreset, -exportPreset, -runBatch, -saveBatch, -migrateLegacyBatches, -cleanupBatchStorage, or related flags.");
}

bool MayaDmxWorkflowCommand::isUndoable() const
{
    return false;
}

void MayaDmxWorkflowCommand::resetState()
{
    workingPreset_ = maya_dmx::ExportPreset{};
    workingEntries_.clear();
    workingName_.clear();
    workingOutputPath_.clear();
    workingExportAll_ = false;
}

MArgDatabase &MayaDmxWorkflowCommand::arguments()
{
    return *arguments_;
}

const MArgDatabase &MayaDmxWorkflowCommand::arguments() const
{
    return *arguments_;
}

bool MayaDmxWorkflowCommand::isFlagSet(const char *flagName) const
{
    return arguments().isFlagSet(flagName);
}

void MayaDmxWorkflowCommand::appendJoinedResult(const MStringArray &items, MString &result) const
{
    for (unsigned int i = 0; i < items.length(); ++i)
    {
        if (i > 0)
        {
            result += "\n";
        }
        result += items[i];
    }
}

MStatus MayaDmxWorkflowCommand::setJoinedResult(const MStringArray &items)
{
    MString result;
    appendJoinedResult(items, result);
    setResult(result);
    return MS::kSuccess;
}

MStatus MayaDmxWorkflowCommand::executeListCommand(MStatus (*operation)(MStringArray &))
{
    workingEntries_.clear();
    MStatus status = operation(workingEntries_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return setJoinedResult(workingEntries_);
}

void MayaDmxWorkflowCommand::populatePresetFromArgs(const char *nameFlag)
{
    arguments().getFlagArgument(nameFlag, 0, workingPreset_.name);

    if (isFlagSet(kOutputDirectoryFlag))
    {
        arguments().getFlagArgument(kOutputDirectoryFlag, 0, workingPreset_.outputDirectory);
    }
    if (isFlagSet(kMaterialRootFlag))
    {
        arguments().getFlagArgument(kMaterialRootFlag, 0, workingPreset_.materialRoot);
    }
    if (isFlagSet(kEncodingFlag))
    {
        arguments().getFlagArgument(kEncodingFlag, 0, workingPreset_.dmxEncoding);
    }
    if (isFlagSet(kUpAxisFlag))
    {
        arguments().getFlagArgument(kUpAxisFlag, 0, workingPreset_.upAxis);
    }
    if (isFlagSet(kExportSkinFlag))
    {
        arguments().getFlagArgument(kExportSkinFlag, 0, workingPreset_.exportSkin);
    }
    if (isFlagSet(kExportDeltaFlag))
    {
        arguments().getFlagArgument(kExportDeltaFlag, 0, workingPreset_.exportDeltaStates);
    }
    if (isFlagSet(kExportMetadataFlag))
    {
        arguments().getFlagArgument(kExportMetadataFlag, 0, workingPreset_.exportMetadata);
    }
}

void MayaDmxWorkflowCommand::collectBatchEntries()
{
    workingEntries_.clear();
    const unsigned int useCount = arguments().numberOfFlagUses(kBatchEntryFlag);
    for (unsigned int i = 0; i < useCount; ++i)
    {
        MArgList entryArgs;
        arguments().getFlagArgumentList(kBatchEntryFlag, i, entryArgs);
        if (entryArgs.length() > 0)
        {
            workingEntries_.append(entryArgs.asString(0));
        }
    }
}

MStatus MayaDmxWorkflowCommand::loadPresetArgument(const char *flagName)
{
    arguments().getFlagArgument(flagName, 0, workingName_);
    workingPreset_ = maya_dmx::ExportPreset{};
    MStatus status = maya_dmx::LoadPreset(workingName_, workingPreset_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return MStatus::kSuccess;
}

MStatus MayaDmxWorkflowCommand::handleLoadPreset()
{
    MStatus status = loadPresetArgument(kLoadPresetFlag);
    if (!status)
    {
        return MStatus::kFailure;
    }

    setResult(maya_dmx::SerializePreset(workingPreset_));
    return MS::kSuccess;
}

MStatus MayaDmxWorkflowCommand::handleSavePreset()
{
    workingPreset_ = maya_dmx::ExportPreset{};
    populatePresetFromArgs(kSavePresetFlag);

    MStatus status = maya_dmx::SavePreset(workingPreset_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    setResult(maya_dmx::SerializePreset(workingPreset_));
    return MS::kSuccess;
}

MStatus MayaDmxWorkflowCommand::handleLoadBatch()
{
    arguments().getFlagArgument(kLoadBatchFlag, 0, workingName_);
    workingEntries_.clear();

    MStatus status = maya_dmx::LoadBatchManifest(workingName_, workingEntries_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return setJoinedResult(workingEntries_);
}

MStatus MayaDmxWorkflowCommand::handleSaveBatch()
{
    arguments().getFlagArgument(kSaveBatchFlag, 0, workingName_);
    collectBatchEntries();

    MStatus status = maya_dmx::SaveBatchManifest(workingName_, workingEntries_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return setJoinedResult(workingEntries_);
}

MStatus MayaDmxWorkflowCommand::handleRunBatch()
{
    arguments().getFlagArgument(kRunBatchFlag, 0, workingName_);
    if (!isFlagSet(kExportPresetFlag))
    {
        return maya_dmx::ReportError("maya_dmx: -exportPreset is required with -runBatch.");
    }

    MStatus status = loadPresetArgument(kExportPresetFlag);
    if (!status)
    {
        return MStatus::kFailure;
    }

    workingEntries_.clear();
    status = maya_dmx::LoadBatchManifest(workingName_, workingEntries_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = maya_dmx::ExecuteBatchExport(workingPreset_, workingEntries_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return setJoinedResult(workingEntries_);
}

MStatus MayaDmxWorkflowCommand::handleExportPreset()
{
    if (!isFlagSet(kExportPathFlag))
    {
        return maya_dmx::ReportError("maya_dmx: -filePath is required with -exportPreset.");
    }

    arguments().getFlagArgument(kExportPathFlag, 0, workingOutputPath_);
    workingExportAll_ = false;
    if (isFlagSet(kExportAllFlag))
    {
        arguments().getFlagArgument(kExportAllFlag, 0, workingExportAll_);
    }

    MStatus status = loadPresetArgument(kExportPresetFlag);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = maya_dmx::ExecuteExport(workingPreset_, workingOutputPath_, !workingExportAll_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    setResult(workingOutputPath_);
    return MS::kSuccess;
}
