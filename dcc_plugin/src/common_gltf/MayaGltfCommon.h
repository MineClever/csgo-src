#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace maya_gltf
{

inline constexpr const char *kPluginVendor = "MineClever";
inline constexpr const char *kPluginVersion = "0.1.0";
inline constexpr const char *kImporterTranslatorName = "cgltf glTF GLB Import";
inline constexpr const char *kExporterTranslatorName = "cgltf glTF GLB Export";

bool HasGltfExtension(const MFileObject &fileObject);
bool HasGlbExtension(const MFileObject &fileObject);
bool HasGltfOrGlbExtension(const MFileObject &fileObject);

MStatus ReportInfo(const MString &message);
MStatus ReportWarning(const MString &message);
MStatus ReportError(const MString &message, MStatus status = MS::kFailure);
MString MakeStubMessage(const char *operationName, const MFileObject &fileObject);

} // namespace maya_gltf
