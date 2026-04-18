#pragma once

#include <cstddef>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
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
MStatus EnsureShaderRegisteredInDefaultShaderList(const MObject &shaderObject);
MStatus ConnectPlugsForce(const MPlug &sourcePlug, const MPlug &destinationPlug);
MStatus AddDagPathToSet(const MDagPath &dagPath, const MObject &setObject);
MStatus AddComponentToSet(const MDagPath &dagPath, const MObject &componentObject, const MObject &setObject);
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
MStatus EnsureSkinClusterBindPose(
    const MDagPathArray &influencePaths,
    const MDagPath &skinnedDagPath);
MStatus EnsureAnimationLayer(
    const MString &layerName,
    bool replaceExisting,
    bool overrideLayer,
    MString *resolvedLayerName = nullptr);
MStatus AddPlugToAnimationLayer(
    const MString &layerName,
    const MPlug &plug);
MStatus ClearAnimationLayerCurve(
    const MString &layerName,
    const MPlug &plug);
MStatus SetKeyframesOnAnimationLayer(
    const MString &layerName,
    const MPlug &plug,
    const double *times,
    const double *values,
    size_t keyCount,
    bool timesAreSeconds);

} // namespace maya_cmd
