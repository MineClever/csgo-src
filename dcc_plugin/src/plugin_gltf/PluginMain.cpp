#include <exporter_gltf/GltfExportTranslator.h>
#include <importer_gltf/GltfImportTranslator.h>
#include <common_gltf/MayaGltfCommon.h>

#include <maya/MFnPlugin.h>

namespace plugin_gltf_detail
{

constexpr const char *kImportOptionsScriptName = "mayaGltfTranslatorImport";
constexpr const char *kExportOptionsScriptName = "mayaGltfTranslatorExport";
constexpr const char *kImportDefaultOptions =
    "importMesh=1;importAnimation=1;importSkin=1;importCamera=1;"
    "importMaterial=1;importVertexColors=1;importNormals=1;importUVs=1;"
    "useSceneRoot=0;restoreDefaultPoseAfterSkin=1;"
    "translateX=0;translateY=0;translateZ=0;"
    "rotateX=0;rotateY=0;rotateZ=0;"
    "scaleX=1.0;scaleY=1.0;scaleZ=1.0;"
    "detectFrameRate=1;frameRate=30";
constexpr const char *kExportDefaultOptions =
    "exportMesh=1;exportAnimation=1;exportCamera=0;exportSkin=1;"
    "exportMaterial=1;exportVertexColors=1;exportNormals=1;exportUVs=1;"
    "flipUvV=1;"
    "translateX=0;translateY=0;translateZ=0;"
    "rotateX=0;rotateY=0;rotateZ=0;"
    "scaleX=1.0;scaleY=1.0;scaleZ=1.0";

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
        return maya_gltf::ReportError(
            MString("maya_gltf: failed to register translator ") + name, status);
    }

    return MS::kSuccess;
}

MStatus DeregisterTranslator(MFnPlugin &plugin, const char *name)
{
    const MStatus status = plugin.deregisterFileTranslator(name);
    if (!status)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: failed to deregister translator ") + name, status);
    }

    return MS::kSuccess;
}

} // namespace plugin_gltf_detail

MStatus initializePlugin(MObject object)
{
    MFnPlugin plugin(object, maya_gltf::kPluginVendor, maya_gltf::kPluginVersion, "Any");

    // Register import translator
    MStatus status = plugin_gltf_detail::RegisterTranslator(
        plugin,
        maya_gltf::kImporterTranslatorName,
        &GltfImportTranslator::Create,
        plugin_gltf_detail::kImportOptionsScriptName,
        plugin_gltf_detail::kImportDefaultOptions);

    if (!status)
    {
        return MStatus::kFailure;
    }

    // Register export translator
    status = plugin_gltf_detail::RegisterTranslator(
        plugin,
        maya_gltf::kExporterTranslatorName,
        &GltfExportTranslator::Create,
        plugin_gltf_detail::kExportOptionsScriptName,
        plugin_gltf_detail::kExportDefaultOptions);

    if (!status)
    {
        plugin.deregisterFileTranslator(maya_gltf::kImporterTranslatorName);
        return MStatus::kFailure;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject object)
{
    MFnPlugin plugin(object);

    MStatus status = plugin_gltf_detail::DeregisterTranslator(
        plugin, maya_gltf::kExporterTranslatorName);
    if (!status) return MStatus::kFailure;

    status = plugin_gltf_detail::DeregisterTranslator(
        plugin, maya_gltf::kImporterTranslatorName);
    if (!status) return MStatus::kFailure;

    return MS::kSuccess;
}
