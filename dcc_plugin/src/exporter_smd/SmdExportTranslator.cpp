#include "SmdExportTranslator.h"
#include "SmdExportSession.h"

#include "../common_smd/MayaSmdCommon.h"

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
    SmdExportSession session(fileObject, options, mode);
    return session.Run();
}
