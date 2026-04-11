#include "SmdImportTranslator.h"
#include "SmdImportSession.h"

#include "../common_smd/MayaSmdCommon.h"

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
    SmdImportSession session(fileObject, options);
    return session.Run();
}
