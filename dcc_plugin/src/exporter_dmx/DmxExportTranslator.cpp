#include "DmxExportTranslator.h"
#include "DmxExportSession.h"

#include "../common_dmx/MayaDmxCommon.h"

#include <exception>

void *DmxExportTranslator::Create()
{
    return new DmxExportTranslator();
}

bool DmxExportTranslator::haveReadMethod() const
{
    return false;
}

bool DmxExportTranslator::haveWriteMethod() const
{
    return true;
}

MString DmxExportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxExportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxExportTranslator::writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode)
{
    try
    {
        DmxExportSession session(fileObject, options, mode);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_dmx::ReportError(MString("maya_dmx: export failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_dmx::ReportError("maya_dmx: export failed with an unknown host exception.");
    }
}
