#pragma once

#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <string>

namespace dcc_node_name
{

constexpr const char *kSourceExportNameAttribute = "sourceExportName";

bool TryReadExportNameOverride(const MObject &nodeObject, std::string &exportName);
std::string ResolveExportNodeName(const MObject &nodeObject, const std::string &fallbackName, bool useExportNameOverride);
MStatus EnsureExportNameOverride(const MObject &nodeObject, const std::string &exportName);

} // namespace dcc_node_name
