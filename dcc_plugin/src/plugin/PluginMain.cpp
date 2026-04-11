#include "../common/MayaDmxCommon.h"
#include "../exporter/DmxExportTranslator.h"
#include "../importer/DmxImportTranslator.h"
#include "../workflow/MayaDmxWorkflowCommand.h"

#include <maya/MFnPlugin.h>

namespace
{
constexpr const char *kWorkflowCommandName = "mayaDmxWorkflow";
constexpr const char *kImportOptionsScriptName = "mayaDmxTranslatorImport";
constexpr const char *kExportOptionsScriptName = "mayaDmxTranslatorExport";
constexpr const char *kImportDefaultOptions = "importMaterials=1;importSkin=1;importDeltaStates=1";
constexpr const char *kExportDefaultOptions = "encoding=text;upAxis=Y;exportSkin=1;exportDeltaStates=1;exportMetadata=1;materialRoot=";

MStatus RegisterTranslator(
    MFnPlugin &plugin,
    const char *name,
    MCreatorFunction creator,
    const char *optionsScriptName = nullptr,
    const char *defaultOptionsString = nullptr)
{
    const MStatus status = plugin.registerFileTranslator(
        name,
        "",
        creator,
        optionsScriptName,
        defaultOptionsString,
        true);
    if (!status)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to register translator ") + name, status);
    }

    return MS::kSuccess;
}

MStatus DeregisterTranslator(MFnPlugin &plugin, const char *name)
{
    const MStatus status = plugin.deregisterFileTranslator(name);
    if (!status)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to deregister translator ") + name, status);
    }

    return MS::kSuccess;
}

MStatus RegisterCommand(MFnPlugin &plugin, const char *name, MCreatorFunction creator, MCreateSyntaxFunction createSyntax)
{
    const MStatus status = plugin.registerCommand(name, creator, createSyntax);
    if (!status)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to register command ") + name, status);
    }
    return MS::kSuccess;
}

MStatus DeregisterCommand(MFnPlugin &plugin, const char *name)
{
    const MStatus status = plugin.deregisterCommand(name);
    if (!status)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to deregister command ") + name, status);
    }
    return MS::kSuccess;
}
}

MStatus initializePlugin(MObject object)
{
    MFnPlugin plugin(object, maya_dmx::kPluginVendor, maya_dmx::kPluginVersion, "Any");

    MStatus status = RegisterTranslator(
        plugin,
        maya_dmx::kImporterTranslatorName,
        &DmxImportTranslator::Create,
        kImportOptionsScriptName,
        kImportDefaultOptions);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = RegisterTranslator(
        plugin,
        maya_dmx::kExporterTranslatorName,
        &DmxExportTranslator::Create,
        kExportOptionsScriptName,
        kExportDefaultOptions);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_dmx::kImporterTranslatorName);
        return MStatus::kFailure;
    }

    status = RegisterCommand(plugin, kWorkflowCommandName, &MayaDmxWorkflowCommand::Create, &MayaDmxWorkflowCommand::CreateSyntax);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_dmx::kExporterTranslatorName);
        plugin.deregisterFileTranslator(maya_dmx::kImporterTranslatorName);
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject object)
{
    (void)object;
    return MS::kSuccess;
}
