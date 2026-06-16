#include "ObjImportTranslator.h"
#include "ObjImportSession.h"

#include <common_obj/MayaObjCommon.h>

#include <exception>

void *ObjImportTranslator::Create()
{
    return new ObjImportTranslator();
}

bool ObjImportTranslator::haveReadMethod() const
{
    return true;
}

bool ObjImportTranslator::haveWriteMethod() const
{
    return false;
}

bool ObjImportTranslator::haveNamespaceSupport() const
{
    return true;
}

bool ObjImportTranslator::canBeOpened() const
{
    return true;
}

MString ObjImportTranslator::defaultExtension() const
{
    return "obj";
}

MPxFileTranslator::MFileKind ObjImportTranslator::identifyFile(
    const MFileObject &fileObject, const char * /*buffer*/, short /*size*/) const
{
    return maya_obj::HasObjExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus ObjImportTranslator::reader(
    const MFileObject &fileObject, const MString &options, FileAccessMode /*mode*/)
{
    try
    {
        ObjImportSession session(fileObject, options);
        return session.Run();
    }
    catch (const std::exception &exception)
    {
        return maya_obj::ReportError(
            MString("maya_obj: import failed with C++ exception: ") + exception.what());
    }
    catch (...)
    {
        return maya_obj::ReportError("maya_obj: import failed with an unknown host exception.");
    }
}
