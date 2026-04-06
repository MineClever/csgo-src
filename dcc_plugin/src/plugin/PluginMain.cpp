#include "../common/MayaDmxCommon.h"
#include "../exporter/DmxExportTranslator.h"
#include "../importer/DmxImportTranslator.h"

#include <maya/MFnPlugin.h>

namespace
{
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

    return maya_dmx::ReportInfo("maya_dmx: registered import/export translators");
}

MStatus uninitializePlugin(MObject object)
{
    MFnPlugin plugin(object);

    MStatus status = DeregisterTranslator(plugin, maya_dmx::kExporterTranslatorName);
    if (!status)
    {
        return status;
    }

    status = DeregisterTranslator(plugin, maya_dmx::kImporterTranslatorName);
    if (!status)
    {
        return status;
    }

    return maya_dmx::ReportInfo("maya_dmx: deregistered import/export translators");
}
