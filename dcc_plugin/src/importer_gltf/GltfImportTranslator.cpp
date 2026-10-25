#include "GltfImportTranslator.h"
#include "GltfImportSession.h"

#include <common_gltf/MayaGltfCommon.h>

void *GltfImportTranslator::Create()
{
    return new GltfImportTranslator();
}

bool GltfImportTranslator::haveReadMethod() const
{
    return true;
}

bool GltfImportTranslator::haveWriteMethod() const
{
    return false;
}

bool GltfImportTranslator::haveNamespaceSupport() const
{
    return false;
}

bool GltfImportTranslator::canBeOpened() const
{
    return false;
}

MString GltfImportTranslator::defaultExtension() const
{
    return MString("gltf");
}

MPxFileTranslator::MFileKind GltfImportTranslator::identifyFile(
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

MStatus GltfImportTranslator::reader(
    const MFileObject &fileObject,
    const MString &options,
    FileAccessMode /*mode*/)
{
    try
    {
        GltfImportSession session(fileObject, options);
        return session.Run();
    }
    catch (const std::exception &ex)
    {
        return maya_gltf::ReportError(
            MString("maya_gltf: unexpected exception during import: ") + ex.what());
    }
}
