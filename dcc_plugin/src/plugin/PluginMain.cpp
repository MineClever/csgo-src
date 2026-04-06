#include "../common/MayaDmxCommon.h"
#include "../exporter/DmxExportTranslator.h"
#include "../importer/DmxImportTranslator.h"
#include "../workflow/MayaDmxWorkflowCommand.h"

#include <maya/MFnPlugin.h>

namespace
{
constexpr const char *kWorkflowCommandName = "mayaDmxWorkflow";

MStatus RegisterTranslator(MFnPlugin &plugin, const char *name, MCreatorFunction creator)
{
    const MStatus status = plugin.registerFileTranslator(name, "", creator);
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

    MStatus status = RegisterTranslator(plugin, maya_dmx::kImporterTranslatorName, &DmxImportTranslator::Create);
    if (!status)
    {
        return status;
    }

    status = RegisterTranslator(plugin, maya_dmx::kExporterTranslatorName, &DmxExportTranslator::Create);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_dmx::kImporterTranslatorName);
        return status;
    }

    status = RegisterCommand(plugin, kWorkflowCommandName, &MayaDmxWorkflowCommand::Create, &MayaDmxWorkflowCommand::CreateSyntax);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_dmx::kExporterTranslatorName);
        plugin.deregisterFileTranslator(maya_dmx::kImporterTranslatorName);
        return status;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject object)
{
    (void)object;
    return MS::kSuccess;
}
