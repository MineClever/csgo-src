#include "MayaObjCommon.h"

#include <cstring>

#include <maya/MGlobal.h>

namespace maya_obj
{
namespace detail
{

bool HasExtension(const MFileObject &fileObject, const char *extension)
{
    const MString lowerName = fileObject.rawFullName().toLowerCase();
    const char *fileName = lowerName.asChar();
    const size_t fileNameLength = strlen(fileName);
    const size_t extensionLength = strlen(extension);
    return fileNameLength >= extensionLength && strcmp(fileName + fileNameLength - extensionLength, extension) == 0;
}

} // namespace detail

bool HasObjExtension(const MFileObject &fileObject)
{
    return detail::HasExtension(fileObject, ".obj");
}

MStatus ReportInfo(const MString &message)
{
    MGlobal::displayInfo(message);
    return MS::kSuccess;
}

MStatus ReportWarning(const MString &message)
{
    MGlobal::displayWarning(message);
    return MS::kSuccess;
}

MStatus ReportError(const MString &message, MStatus status)
{
    MGlobal::displayError(message);
    return status;
}

MString MakeStubMessage(const char *operationName, const MFileObject &fileObject)
{
    MString message("maya_obj: ");
    message += operationName;
    message += " is not implemented yet for file ";
    message += fileObject.rawFullName();
    return message;
}

} // namespace maya_obj
