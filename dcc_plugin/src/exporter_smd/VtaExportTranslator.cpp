#include "VtaExportTranslator.h"

#include "VtaExportSession.h"

#include <common_smd/MayaSmdCommon.h>

#include <exception>

void *VtaExportTranslator::Create()
{
    return new VtaExportTranslator();
}

bool VtaExportTranslator::haveReadMethod() const
{
    return false;
}

bool VtaExportTranslator::haveWriteMethod() const
{
    return true;
}

MString VtaExportTranslator::defaultExtension() const
{
    return "vta";
}

MPxFileTranslator::MFileKind VtaExportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_smd::HasVtaExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus VtaExportTranslator::writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode)
{
    try
    {
        VtaExportSession session(fileObject, options, mode);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_smd::ReportError(MString("maya_smd: VTA export failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_smd::ReportError("maya_smd: VTA export failed with an unknown host exception.");
    }
}
