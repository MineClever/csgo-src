#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace maya_dmx
{
inline constexpr const char *kPluginVendor = "csgo-src";
inline constexpr const char *kPluginVersion = "0.1.0";
inline constexpr const char *kImporterTranslatorName = "Valve DMX Import";
inline constexpr const char *kExporterTranslatorName = "Valve DMX Export";

bool HasDmxExtension(const MFileObject &fileObject);
MStatus ReportInfo(const MString &message);
MStatus ReportWarning(const MString &message);
MStatus ReportError(const MString &message, MStatus status = MS::kFailure);
MString MakeStubMessage(const char *operationName, const MFileObject &fileObject);
}
