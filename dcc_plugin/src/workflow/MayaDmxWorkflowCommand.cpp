#include "MayaDmxWorkflowCommand.h"

#include "MayaDmxWorkflow.h"
#include "../common/MayaDmxCommon.h"

#include <maya/MArgList.h>
#include <maya/MArgDatabase.h>
#include <maya/MGlobal.h>

namespace
{
constexpr const char *kCommandName = "mayaDmxWorkflow";
constexpr const char *kSavePresetFlag = "-sp";
constexpr const char *kSavePresetLongFlag = "-savePreset";
constexpr const char *kLoadPresetFlag = "-lp";
constexpr const char *kLoadPresetLongFlag = "-loadPreset";
constexpr const char *kDeletePresetFlag = "-dp";
constexpr const char *kDeletePresetLongFlag = "-deletePreset";
constexpr const char *kListPresetsFlag = "-ls";
constexpr const char *kListPresetsLongFlag = "-listPresets";
constexpr const char *kOutputDirectoryFlag = "-od";
constexpr const char *kOutputDirectoryLongFlag = "-outputDirectory";
constexpr const char *kMaterialRootFlag = "-mr";
constexpr const char *kMaterialRootLongFlag = "-materialRoot";
constexpr const char *kEncodingFlag = "-enc";
constexpr const char *kEncodingLongFlag = "-encoding";
constexpr const char *kUpAxisFlag = "-ua";
constexpr const char *kUpAxisLongFlag = "-upAxis";
constexpr const char *kExportSkinFlag = "-es";
constexpr const char *kExportSkinLongFlag = "-exportSkin";
constexpr const char *kExportDeltaFlag = "-eds";
constexpr const char *kExportDeltaLongFlag = "-exportDeltaStates";
constexpr const char *kSaveBatchFlag = "-sb";
constexpr const char *kSaveBatchLongFlag = "-saveBatch";
constexpr const char *kLoadBatchFlag = "-lb";
constexpr const char *kLoadBatchLongFlag = "-loadBatch";
constexpr const char *kDeleteBatchFlag = "-db";
constexpr const char *kDeleteBatchLongFlag = "-deleteBatch";
constexpr const char *kListBatchesFlag = "-lbt";
constexpr const char *kListBatchesLongFlag = "-listBatches";
constexpr const char *kBatchEntryFlag = "-be";
constexpr const char *kBatchEntryLongFlag = "-batchEntry";

void AppendJoinedResult(const MStringArray &items, MString &result)
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
}

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
    syntax.addFlag(kSaveBatchFlag, kSaveBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kLoadBatchFlag, kLoadBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kDeleteBatchFlag, kDeleteBatchLongFlag, MSyntax::kString);
    syntax.addFlag(kListBatchesFlag, kListBatchesLongFlag);
    syntax.addFlag(kBatchEntryFlag, kBatchEntryLongFlag, MSyntax::kString);
    syntax.makeFlagMultiUse(kBatchEntryFlag);
    return syntax;
}

MStatus MayaDmxWorkflowCommand::doIt(const MArgList &args)
{
    MArgDatabase arguments(syntax(), args);

    if (arguments.isFlagSet(kListPresetsFlag))
    {
        MStringArray names;
        MStatus status = maya_dmx::ListPresetNames(names);
        if (!status)
        {
            return status;
        }

        MString result;
        AppendJoinedResult(names, result);
        setResult(result);
        return MS::kSuccess;
    }

    if (arguments.isFlagSet(kListBatchesFlag))
    {
        MStringArray names;
        MStatus status = maya_dmx::ListBatchManifestNames(names);
        if (!status)
        {
            return status;
        }

        MString result;
        AppendJoinedResult(names, result);
        setResult(result);
        return MS::kSuccess;
    }

    if (arguments.isFlagSet(kLoadPresetFlag))
    {
        MString name;
        arguments.getFlagArgument(kLoadPresetFlag, 0, name);

        maya_dmx::ExportPreset preset;
        MStatus status = maya_dmx::LoadPreset(name, preset);
        if (!status)
        {
            return status;
        }

        setResult(maya_dmx::SerializePreset(preset));
        return MS::kSuccess;
    }

    if (arguments.isFlagSet(kDeletePresetFlag))
    {
        MString name;
        arguments.getFlagArgument(kDeletePresetFlag, 0, name);
        return maya_dmx::DeletePreset(name);
    }

    if (arguments.isFlagSet(kSavePresetFlag))
    {
        maya_dmx::ExportPreset preset;
        arguments.getFlagArgument(kSavePresetFlag, 0, preset.name);

        if (arguments.isFlagSet(kOutputDirectoryFlag))
        {
            arguments.getFlagArgument(kOutputDirectoryFlag, 0, preset.outputDirectory);
        }
        if (arguments.isFlagSet(kMaterialRootFlag))
        {
            arguments.getFlagArgument(kMaterialRootFlag, 0, preset.materialRoot);
        }
        if (arguments.isFlagSet(kEncodingFlag))
        {
            arguments.getFlagArgument(kEncodingFlag, 0, preset.dmxEncoding);
        }
        if (arguments.isFlagSet(kUpAxisFlag))
        {
            arguments.getFlagArgument(kUpAxisFlag, 0, preset.upAxis);
        }
        if (arguments.isFlagSet(kExportSkinFlag))
        {
            arguments.getFlagArgument(kExportSkinFlag, 0, preset.exportSkin);
        }
        if (arguments.isFlagSet(kExportDeltaFlag))
        {
            arguments.getFlagArgument(kExportDeltaFlag, 0, preset.exportDeltaStates);
        }

        MStatus status = maya_dmx::SavePreset(preset);
        if (!status)
        {
            return status;
        }

        setResult(maya_dmx::SerializePreset(preset));
        return MS::kSuccess;
    }

    if (arguments.isFlagSet(kLoadBatchFlag))
    {
        MString name;
        arguments.getFlagArgument(kLoadBatchFlag, 0, name);

        MStringArray entries;
        MStatus status = maya_dmx::LoadBatchManifest(name, entries);
        if (!status)
        {
            return status;
        }

        MString result;
        AppendJoinedResult(entries, result);
        setResult(result);
        return MS::kSuccess;
    }

    if (arguments.isFlagSet(kDeleteBatchFlag))
    {
        MString name;
        arguments.getFlagArgument(kDeleteBatchFlag, 0, name);
        return maya_dmx::DeleteBatchManifest(name);
    }

    if (arguments.isFlagSet(kSaveBatchFlag))
    {
        MString name;
        arguments.getFlagArgument(kSaveBatchFlag, 0, name);

        MStringArray entries;
        const unsigned int useCount = arguments.numberOfFlagUses(kBatchEntryFlag);
        for (unsigned int i = 0; i < useCount; ++i)
        {
            MArgList entryArgs;
            arguments.getFlagArgumentList(kBatchEntryFlag, i, entryArgs);
            if (entryArgs.length() > 0)
            {
                entries.append(entryArgs.asString(0));
            }
        }

        MStatus status = maya_dmx::SaveBatchManifest(name, entries);
        if (!status)
        {
            return status;
        }

        MString result;
        AppendJoinedResult(entries, result);
        setResult(result);
        return MS::kSuccess;
    }

    return maya_dmx::ReportError(
        MString("maya_dmx: no workflow action specified. Use ") + kCommandName + " with -savePreset, -loadPreset, -saveBatch, or related flags.");
}

bool MayaDmxWorkflowCommand::isUndoable() const
{
    return false;
}
