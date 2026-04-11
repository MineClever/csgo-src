#pragma once

#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include <filesystem>
#include <string>
#include <vector>

namespace maya_dmx
{
namespace workflow_support
{
constexpr const char *kPresetVarPrefix = "maya_dmx_preset_";
constexpr const char *kBatchVarPrefix = "maya_dmx_batch_";
constexpr const char *kBatchManifestDirectoryName = "maya_dmx_workflow";
constexpr const char *kBatchManifestExtension = ".batch";

MString EscapeValue(const MString &value);
MString UnescapeValue(const MString &value);
MString EncodeHexString(const MString &value);
MString EncodeHexBytes(const std::string &value);
bool DecodeHexString(const MString &value, MString &decoded);
bool DecodeHexBytes(const std::string &value, std::string &decoded);
std::vector<MString> SplitEscaped(const MString &text, char delimiter);
MString MakeOptionVarName(const char *prefix, const MString &name);

MStatus GetMayaUserPrefDirectory(std::filesystem::path &directory);
MStatus GetBatchManifestDirectory(std::filesystem::path &directory);
std::filesystem::path GetBatchManifestPath(const MString &name);
MString JoinPath(const MString &baseDirectory, const MString &relativePath);
MStatus EnsureParentDirectory(const MString &path);

MStatus SetOptionVarString(const MString &name, const MString &value);
bool GetOptionVarString(const MString &name, MString &value);
MStatus RemoveOptionVar(const MString &name);
MStatus CollectOptionVars(const char *prefix, MStringArray &names);
}
}
