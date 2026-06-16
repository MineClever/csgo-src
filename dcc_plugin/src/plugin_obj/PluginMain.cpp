#include <importer_obj/ObjImportTranslator.h>
#include <common_obj/MayaObjCommon.h>

#include <maya/MFnPlugin.h>

namespace plugin_obj_detail
{

constexpr const char *kImportOptionsScriptName = "mayaObjTranslatorImport";
constexpr const char *kImportDefaultOptions =
    "useSceneRoot=0;"
    "translateX=0;translateY=0;translateZ=0;"
    "rotateX=0;rotateY=0;rotateZ=0;"
    "scaleX=1;scaleY=1;scaleZ=1;"
    "flipUvV=1";

MStatus RegisterImportTranslator(MFnPlugin &plugin)
{
    const MStatus status = plugin.registerFileTranslator(
        maya_obj::kImporterTranslatorName,
        "",
        &ObjImportTranslator::Create,
        kImportOptionsScriptName,
        kImportDefaultOptions,
        true);

    if (!status)
    {
        return maya_obj::ReportError(
            MString("maya_obj: failed to register translator ") + maya_obj::kImporterTranslatorName,
            status);
    }

    return MS::kSuccess;
}

MStatus DeregisterImportTranslator(MFnPlugin &plugin)
{
    const MStatus status = plugin.deregisterFileTranslator(maya_obj::kImporterTranslatorName);
    if (!status)
    {
        return maya_obj::ReportError(
            MString("maya_obj: failed to deregister translator ") + maya_obj::kImporterTranslatorName,
            status);
    }

    return MS::kSuccess;
}

} // namespace plugin_obj_detail

MStatus initializePlugin(MObject object)
{
    MFnPlugin plugin(object, maya_obj::kPluginVendor, maya_obj::kPluginVersion, "Any");

    return plugin_obj_detail::RegisterImportTranslator(plugin);
}

MStatus uninitializePlugin(MObject object)
{
    MFnPlugin plugin(object);

    return plugin_obj_detail::DeregisterImportTranslator(plugin);
}
