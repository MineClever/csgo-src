#include "SmdExportTranslator.h"
#include "SmdExportSession.h"

#include <common_smd/MayaSmdCommon.h>

#include <exception>

void *SmdExportTranslator::Create()
{
    return new SmdExportTranslator();
}

bool SmdExportTranslator::haveReadMethod() const
{
    return false;
}

bool SmdExportTranslator::haveWriteMethod() const
{
    return true;
}

MString SmdExportTranslator::defaultExtension() const
{
    return "smd";
}

MPxFileTranslator::MFileKind SmdExportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_smd::HasSmdExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus SmdExportTranslator::writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode)
{
    try
    {
        SmdExportSession session(fileObject, options, mode);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_smd::ReportError(MString("maya_smd: export failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_smd::ReportError("maya_smd: export failed with an unknown host exception.");
    }
}
