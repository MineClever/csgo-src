#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace maya_common
{

/// Case-insensitive extension check.
bool HasExtension(const MFileObject &fileObject, const char *extension);

/// Display an info message via MGlobal.
MStatus ReportInfo(const MString &message);

/// Display a warning message via MGlobal.
MStatus ReportWarning(const MString &message);

/// Display an error message via MGlobal.
MStatus ReportError(const MString &message, MStatus status = MS::kFailure);

/// Build a "not implemented yet" stub message.
MString MakeStubMessage(const char *formatName, const char *operationName, const MFileObject &fileObject);

} // namespace maya_common
