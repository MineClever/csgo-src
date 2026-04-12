#include <exporter_smd/SmdExportTranslator.h>
#include <importer_smd/SmdImportTranslator.h>
#include <common_smd/MayaSmdCommon.h>

#include <maya/MFnPlugin.h>

namespace
{
constexpr const char *kImportOptionsScriptName = "mayaSmdTranslatorImport";
constexpr const char *kImportDefaultOptions = "useSceneRoot=0;importMode=create;translateX=0;translateY=0;translateZ=0;rotateX=0;rotateY=0;rotateZ=0;scaleX=1;scaleY=1;scaleZ=1";

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

    MStatus status = RegisterTranslator(
        plugin,
        maya_smd::kImporterTranslatorName,
        &SmdImportTranslator::Create,
        kImportOptionsScriptName,
        kImportDefaultOptions);
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
