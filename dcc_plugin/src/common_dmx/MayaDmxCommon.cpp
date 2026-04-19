#include "MayaDmxCommon.h"

#include <cstring>

#include <maya/MGlobal.h>

namespace maya_dmx
{
bool HasDmxExtension(const MFileObject &fileObject)
{
    const MString lowerName = fileObject.rawFullName().toLowerCase();
    const char *fileName = lowerName.asChar();
    const size_t fileNameLength = strlen(fileName);
    for (const char *extension : {".dmx", ".dmxb", ".dmxbin"})
    {
        const size_t extensionLength = strlen(extension);
        if (fileNameLength >= extensionLength && strcmp(fileName + fileNameLength - extensionLength, extension) == 0)
        {
            return true;
        }
    }

    return false;
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
    MString message("maya_dmx: ");
    message += operationName;
    message += " is not implemented yet for file ";
    message += fileObject.rawFullName();
    return message;
}
}
