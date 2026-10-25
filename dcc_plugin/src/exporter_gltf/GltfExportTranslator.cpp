#include "GltfExportTranslator.h"
#include "GltfExportSession.h"

#include <common_gltf/MayaGltfCommon.h>

void *GltfExportTranslator::Create()
{
    return new GltfExportTranslator();
}

bool GltfExportTranslator::haveReadMethod() const
{
    return false;
}

bool GltfExportTranslator::haveWriteMethod() const
{
    return true;
}

bool GltfExportTranslator::haveNamespaceSupport() const
{
    return false;
}

bool GltfExportTranslator::canBeOpened() const
{
    return false;
}

MString GltfExportTranslator::defaultExtension() const
{
    return MString("gltf");
}

MPxFileTranslator::MFileKind GltfExportTranslator::identifyFile(
    const MFileObject &fileObject,
    const char * /*buffer*/,
    short /*size*/) const
{
    if (maya_gltf::HasGltfOrGlbExtension(fileObject))
    {
        return kCouldBeMyFileType;
    }

    return kNotMyFileType;
}

MStatus GltfExportTranslator::writer(
    const MFileObject &fileObject,
    const MString &options,
    FileAccessMode mode)
{
    try
    {
        GltfExportSession session(fileObject, options, mode);
        return session.Run();
    }
    catch (const std::exception &ex)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: unexpected exception during export: ") + ex.what());
    }
}
