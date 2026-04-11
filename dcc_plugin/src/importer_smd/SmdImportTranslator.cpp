#include "SmdImportTranslator.h"
#include "SmdImportSession.h"

#include <common_smd/MayaSmdCommon.h>

#include <exception>

void *SmdImportTranslator::Create()
{
    return new SmdImportTranslator();
}

bool SmdImportTranslator::haveReadMethod() const
{
    return true;
}

bool SmdImportTranslator::haveWriteMethod() const
{
    return false;
}

bool SmdImportTranslator::canBeOpened() const
{
    return true;
}

MString SmdImportTranslator::defaultExtension() const
{
    return "smd";
}

MPxFileTranslator::MFileKind SmdImportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_smd::HasSmdExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus SmdImportTranslator::reader(const MFileObject &fileObject, const MString &options, FileAccessMode)
{
    try
    {
        SmdImportSession session(fileObject, options);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_smd::ReportError(MString("maya_smd: import failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_smd::ReportError("maya_smd: import failed with an unknown host exception.");
    }
}
