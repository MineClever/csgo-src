#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace maya_smd
{
inline constexpr const char *kPluginVendor = "MineClever";
inline constexpr const char *kPluginVersion = "0.1.0";
inline constexpr const char *kImporterTranslatorName = "Valve SMD Import";
inline constexpr const char *kExporterTranslatorName = "Valve SMD Export";
inline constexpr const char *kVtaImporterTranslatorName = "Valve VTA Import";
inline constexpr const char *kVtaExporterTranslatorName = "Valve VTA Export";

bool HasSmdExtension(const MFileObject &fileObject);
bool HasVtaExtension(const MFileObject &fileObject);
MStatus ReportInfo(const MString &message);
MStatus ReportWarning(const MString &message);
MStatus ReportError(const MString &message, MStatus status = MS::kFailure);
MString MakeStubMessage(const char *operationName, const MFileObject &fileObject);
}
