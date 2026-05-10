#include <common_dmx/MayaDmxCommon.h>
#include <exporter_dmx/DmxExportTranslator.h>
#include <importer_dmx/DmxImportTranslator.h>
#include <workflow/MayaDmxWorkflowCommand.h>

#include <maya/MFnPlugin.h>

namespace plugin_dmx_detail
{
constexpr const char *kWorkflowCommandName = "mayaDmxWorkflow";
constexpr const char *kImportOptionsScriptName = "mayaDmxTranslatorImport";
constexpr const char *kExportOptionsScriptName = "mayaDmxTranslatorExport";
constexpr const char *kImportDefaultOptions = "importMaterials=1;importSkin=1;importDeltaStates=1;useSceneRoot=0;importMode=create;translateX=0;translateY=0;translateZ=0;rotateX=0;rotateY=0;rotateZ=0;scaleX=1;scaleY=1;scaleZ=1;recordExportName=0";
constexpr const char *kExportDefaultOptions = "encoding=text;upAxis=Y;exportSkin=1;exportDeltaStates=1;exportMetadata=1;useExportNameOverride=0;materialRoot=;translateX=0;translateY=0;translateZ=0;rotateX=0;rotateY=0;rotateZ=0;scaleX=1;scaleY=1;scaleZ=1";

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
} // namespace plugin_dmx_detail

MStatus initializePlugin(MObject object)
{
    MFnPlugin plugin(object, maya_dmx::kPluginVendor, maya_dmx::kPluginVersion, "Any");

    MStatus status = plugin_dmx_detail::RegisterTranslator(
        plugin,
        maya_dmx::kImporterTranslatorName,
        &DmxImportTranslator::Create,
        plugin_dmx_detail::kImportOptionsScriptName,
        plugin_dmx_detail::kImportDefaultOptions);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = plugin_dmx_detail::RegisterTranslator(
        plugin,
        maya_dmx::kExporterTranslatorName,
        &DmxExportTranslator::Create,
        plugin_dmx_detail::kExportOptionsScriptName,
        plugin_dmx_detail::kExportDefaultOptions);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_dmx::kImporterTranslatorName);
        return MStatus::kFailure;
    }

    status = plugin_dmx_detail::RegisterCommand(
        plugin,
        plugin_dmx_detail::kWorkflowCommandName,
        &MayaDmxWorkflowCommand::Create,
        &MayaDmxWorkflowCommand::CreateSyntax);
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
    MFnPlugin plugin(object);

    MStatus status = plugin_dmx_detail::DeregisterCommand(plugin, plugin_dmx_detail::kWorkflowCommandName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = plugin_dmx_detail::DeregisterTranslator(plugin, maya_dmx::kExporterTranslatorName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = plugin_dmx_detail::DeregisterTranslator(plugin, maya_dmx::kImporterTranslatorName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}
