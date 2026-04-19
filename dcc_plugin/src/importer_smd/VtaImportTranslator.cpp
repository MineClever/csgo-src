#include "VtaImportTranslator.h"

#include "SmdImportSession.h"

#include <common_smd/MayaSmdCommon.h>

#include <exception>

void *VtaImportTranslator::Create()
{
    return new VtaImportTranslator();
}

bool VtaImportTranslator::haveReadMethod() const
{
    return true;
}

bool VtaImportTranslator::haveWriteMethod() const
{
    return false;
}

bool VtaImportTranslator::haveNamespaceSupport() const
{
    return true;
}

bool VtaImportTranslator::canBeOpened() const
{
    return true;
}

MString VtaImportTranslator::defaultExtension() const
{
    return "vta";
}

MPxFileTranslator::MFileKind VtaImportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_smd::HasVtaExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus VtaImportTranslator::reader(const MFileObject &fileObject, const MString &options, FileAccessMode)
{
    try
    {
        SmdImportSession session(fileObject, options);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_smd::ReportError(MString("maya_smd: VTA import failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_smd::ReportError("maya_smd: VTA import failed with an unknown host exception.");
    }
}
