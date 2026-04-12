#include "MayaCommandUtils.h"

#include <maya/MDGModifier.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>

namespace maya_cmd
{

bool TryGetNodeByName(const MString &nodeName, MObject &nodeObject)
{
    nodeObject = MObject::kNullObj;

    MSelectionList selection;
    if (selection.add(nodeName) != MS::kSuccess)
    {
        return false;
    }

    return selection.getDependNode(0, nodeObject) == MS::kSuccess && !nodeObject.isNull();
}

MStatus DeleteNodeByName(const MString &nodeName)
{
    MObject nodeObject;
    if (!TryGetNodeByName(nodeName, nodeObject))
    {
        return MS::kSuccess;
    }

    return MGlobal::deleteNode(nodeObject);
}

MStatus DuplicateDagNode(const MDagPath &sourcePath, MObject &duplicateObject, MDagPath *duplicatePath)
{
    duplicateObject = MObject::kNullObj;

    MStatus status;
    MFnDagNode dagNode(sourcePath, &status);
    if (!status)
    {
        return status;
    }

    duplicateObject = dagNode.duplicate(false, false, &status);
    if (!status)
    {
        return status;
    }

    if (duplicatePath)
    {
        status = MDagPath::getAPathTo(duplicateObject, *duplicatePath);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

MStatus GetNodeAliasList(const MObject &nodeObject, MStringArray &aliasPairs)
{
    aliasPairs.clear();

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return status;
    }

    nodeFn.getAliasList(aliasPairs, &status);
    return status;
}

MStatus SetNodePlugAlias(const MObject &nodeObject, const MPlug &plug, const MString &aliasName)
{
    if (nodeObject.isNull() || plug.isNull() || aliasName.length() == 0)
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return status;
    }

    const MString attrName = plug.partialName(false, false, false, false, false, true, &status);
    if (!status)
    {
        return status;
    }

    nodeFn.setAlias(aliasName, attrName, plug, true, &status);
    return status;
}

MStatus CreateNamedDependencyNode(const MString &typeName, const MString &nodeName, MObject &nodeObject)
{
    MStatus status;
    MFnDependencyNode nodeFn;
    nodeObject = nodeFn.create(typeName, nodeName, &status);
    return status;
}

MStatus EnsureRenderableShadingGroup(const MString &nodeName, MObject &setObject)
{
    setObject = MObject::kNullObj;

    MObject existingObject;
    if (TryGetNodeByName(nodeName, existingObject) && existingObject.hasFn(MFn::kSet))
    {
        MStatus status;
        MFnSet existingSet(existingObject, &status);
        if (status && existingSet.restriction() == MFnSet::kRenderableOnly)
        {
            setObject = existingObject;
            return MS::kSuccess;
        }
    }

    MSelectionList emptyList;
    MFnSet setFn;
    MStatus status;
    setObject = setFn.create(emptyList, MFnSet::kRenderableOnly, &status);
    if (!status)
    {
        return status;
    }

    setFn.setName(nodeName, &status);
    return status;
}

MStatus ConnectPlugsForce(const MPlug &sourcePlug, const MPlug &destinationPlug)
{
    if (sourcePlug.isNull() || destinationPlug.isNull())
    {
        return MStatus::kFailure;
    }

    MStatus status;
    MPlugArray sourceConnections;
    destinationPlug.connectedTo(sourceConnections, true, false, &status);
    if (!status)
    {
        return status;
    }

    MDGModifier modifier;
    for (unsigned int sourceIndex = 0; sourceIndex < sourceConnections.length(); ++sourceIndex)
    {
        status = modifier.disconnect(sourceConnections[sourceIndex], destinationPlug);
        if (!status)
        {
            return status;
        }
    }

    status = modifier.connect(sourcePlug, destinationPlug);
    if (!status)
    {
        return status;
    }

    return modifier.doIt();
}

MStatus AddDagPathToSet(const MDagPath &dagPath, const MObject &setObject)
{
    MStatus status;
    MFnSet setFn(setObject, &status);
    if (!status)
    {
        return status;
    }

    return setFn.addMember(dagPath);
}

MStatus GetPrunedHistory(const MString &nodePath, MStringArray &historyNames)
{
    historyNames.clear();

    // Keep the MEL command wrapped here for now. `listHistory -pruneDagObjects true`
    // has scene-graph filtering semantics that still need a one-by-one API parity pass
    // before replacing it with MItDependencyGraph.
    return MGlobal::executeCommand(
        MString("listHistory -pruneDagObjects true \"") + nodePath + "\"",
        historyNames,
        false,
        false);
}

MStatus GetMayaUserPrefDirectory(MString &userPrefDir)
{
    userPrefDir.clear();

    // Keep the MEL command wrapped here for now. The current workflow expects the
    // exact `internalVar -userPrefDir` result and we have not yet identified a fully
    // equivalent public C++ API for this path lookup.
    return MGlobal::executeCommand("internalVar -userPrefDir", userPrefDir);
}

MStatus ListOptionVarNames(MStringArray &optionVarNames)
{
    optionVarNames.clear();

    // Keep the MEL command wrapped here for now. Maya exposes optionVar value APIs,
    // but enumerating all optionVar names still relies on the command layer.
    return MGlobal::executeCommand("optionVar -list", optionVarNames);
}

MStatus RegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MStringArray &result)
{
    result.clear();

    // Keep the MEL command wrapped here for now. `sculptTarget -regenerate` does not
    // have a direct C++ API equivalent with matching behavior for temporary target mesh creation.
    MString command("sculptTarget -e -regenerate true -target ");
    command += static_cast<int>(weightIndex);
    command += " \"";
    command += blendShapeNodeName;
    command += "\"";
    return MGlobal::executeCommand(command, result, false, false);
}

} // namespace maya_cmd
