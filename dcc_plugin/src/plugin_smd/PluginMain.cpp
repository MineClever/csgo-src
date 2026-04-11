#include "../exporter_smd/SmdExportTranslator.h"
#include "../importer_smd/SmdImportTranslator.h"
#include "../common_smd/MayaSmdCommon.h"

#include <maya/MFnPlugin.h>

namespace
{
MStatus RegisterTranslator(MFnPlugin &plugin, const char *name, MCreatorFunction creator)
{
    const MStatus status = plugin.registerFileTranslator(
        name,
        "",
        creator,
        nullptr,
        nullptr,
        true);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to register translator ") + name, status);
    }

    return MS::kSuccess;
}

MStatus DeregisterTranslator(MFnPlugin &plugin, const char *name)
{
    const MStatus status = plugin.deregisterFileTranslator(name);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to deregister translator ") + name, status);
    }

    return MS::kSuccess;
}
}

MStatus initializePlugin(MObject object)
{
    MFnPlugin plugin(object, maya_smd::kPluginVendor, maya_smd::kPluginVersion, "Any");

    MStatus status = RegisterTranslator(plugin, maya_smd::kImporterTranslatorName, &SmdImportTranslator::Create);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = RegisterTranslator(plugin, maya_smd::kExporterTranslatorName, &SmdExportTranslator::Create);
    if (!status)
    {
        plugin.deregisterFileTranslator(maya_smd::kImporterTranslatorName);
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject object)
{
    MFnPlugin plugin(object);

    MStatus status = DeregisterTranslator(plugin, maya_smd::kExporterTranslatorName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = DeregisterTranslator(plugin, maya_smd::kImporterTranslatorName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}
