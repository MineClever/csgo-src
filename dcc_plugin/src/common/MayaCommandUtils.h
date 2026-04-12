#pragma once

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

namespace maya_cmd
{

bool TryGetNodeByName(const MString &nodeName, MObject &nodeObject);
MStatus DeleteNodeByName(const MString &nodeName);
MStatus DuplicateDagNode(const MDagPath &sourcePath, MObject &duplicateObject, MDagPath *duplicatePath = nullptr);
MStatus GetNodeAliasList(const MObject &nodeObject, MStringArray &aliasPairs);
MStatus SetNodePlugAlias(const MObject &nodeObject, const MPlug &plug, const MString &aliasName);
MStatus CreateNamedDependencyNode(const MString &typeName, const MString &nodeName, MObject &nodeObject);
MStatus EnsureRenderableShadingGroup(const MString &nodeName, MObject &setObject);
MStatus ConnectPlugsForce(const MPlug &sourcePlug, const MPlug &destinationPlug);
MStatus AddDagPathToSet(const MDagPath &dagPath, const MObject &setObject);
MStatus GetPrunedHistory(const MString &nodePath, MStringArray &historyNames);
MStatus GetMayaUserPrefDirectory(MString &userPrefDir);
MStatus ListOptionVarNames(MStringArray &optionVarNames);
MStatus RegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MStringArray &result);
MStatus AddSkinClusterInfluence(
    const MString &skinClusterNodeName,
    const MDagPath &influencePath);

} // namespace maya_cmd
