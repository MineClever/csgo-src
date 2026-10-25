// MayaGltfCommon.h/.cpp now delegates to common/MayaCommon for the shared
// reporting and extension-checking logic, keeping only the glTF-specific
// constants and extension wrappers.

#include "MayaGltfCommon.h"

#include <common/MayaCommon.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace maya_gltf
{

bool HasGltfExtension(const MFileObject &fileObject)
{
    return maya_common::HasExtension(fileObject, ".gltf");
}

bool HasGlbExtension(const MFileObject &fileObject)
{
    return maya_common::HasExtension(fileObject, ".glb");
}

bool HasGltfOrGlbExtension(const MFileObject &fileObject)
{
    return HasGltfExtension(fileObject) || HasGlbExtension(fileObject);
}

MStatus ReportInfo(const MString &message)
{
    return maya_common::ReportInfo(MString("maya_gltf: ") + message);
}

MStatus ReportWarning(const MString &message)
{
    return maya_common::ReportWarning(MString("maya_gltf: ") + message);
}

MStatus ReportError(const MString &message, MStatus status)
{
    return maya_common::ReportError(MString("maya_gltf: ") + message, status);
}

MString MakeStubMessage(const char *operationName, const MFileObject &fileObject)
{
    return maya_common::MakeStubMessage("maya_gltf", operationName, fileObject);
}

} // namespace maya_gltf
