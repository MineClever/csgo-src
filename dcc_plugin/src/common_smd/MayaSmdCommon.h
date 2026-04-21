#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace maya_smd
{
inline constexpr const char *kPluginVendor = "MineClever";
inline constexpr const char *kPluginVersion = "0.1.0";
inline constexpr const char *kImporterTranslatorName = "Source SMD Import";
inline constexpr const char *kExporterTranslatorName = "Source SMD Export";
inline constexpr const char *kVtaImporterTranslatorName = "Source VTA Import";
inline constexpr const char *kVtaExporterTranslatorName = "Source VTA Export";

bool HasSmdExtension(const MFileObject &fileObject);
bool HasVtaExtension(const MFileObject &fileObject);
MStatus ReportInfo(const MString &message);
MStatus ReportWarning(const MString &message);
MStatus ReportError(const MString &message, MStatus status = MS::kFailure);
MString MakeStubMessage(const char *operationName, const MFileObject &fileObject);
}
