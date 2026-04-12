#include "MayaCommandUtils.h"

#include <maya/MDGModifier.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>

#include <unordered_set>

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
        return MS::kFailure;
    }

    duplicateObject = dagNode.duplicate(false, false, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    if (duplicatePath)
    {
        status = MDagPath::getAPathTo(duplicateObject, *duplicatePath);
        if (!status)
        {
            return MS::kFailure;
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
        return MS::kFailure;
    }

    nodeFn.getAliasList(aliasPairs, &status);
    return status ? MS::kSuccess : MS::kFailure;
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
        return MS::kFailure;
    }

    const MString attrName = plug.partialName(false, false, false, false, false, true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    nodeFn.setAlias(aliasName, attrName, plug, true, &status);
    return status.error() ? MS::kSuccess : MS::kFailure;
}

MStatus CreateNamedDependencyNode(const MString &typeName, const MString &nodeName, MObject &nodeObject)
{
    MStatus status;
    MFnDependencyNode nodeFn;
    nodeObject = nodeFn.create(typeName, nodeName, &status);
    return status ? MS::kSuccess : MS::kFailure;
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
        return MS::kFailure;
    }

    setFn.setName(nodeName, &status);
    return status ? MS::kSuccess : MS::kFailure;
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
        return MS::kFailure;
    }

    MDGModifier modifier;
    for (unsigned int sourceIndex = 0; sourceIndex < sourceConnections.length(); ++sourceIndex)
    {
        status = modifier.disconnect(sourceConnections[sourceIndex], destinationPlug);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    status = modifier.connect(sourcePlug, destinationPlug);
    if (!status)
    {
        return MS::kFailure;
    }

    return modifier.doIt();
}

MStatus AddDagPathToSet(const MDagPath &dagPath, const MObject &setObject)
{
    MStatus status;
    MFnSet setFn(setObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    return setFn.addMember(dagPath);
}

MStatus GetPrunedHistory(const MString &nodePath, MStringArray &historyNames)
{
    historyNames.clear();

    MSelectionList selection;
    MStatus status = selection.add(nodePath);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject rootObject;
    status = selection.getDependNode(0, rootObject);
    if (!status || rootObject.isNull())
    {
        return MS::kFailure;
    }

    MItDependencyGraph iterator(
        rootObject,
        MFn::kInvalid,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MS::kFailure;
    }

    std::unordered_set<std::string> seenNames;
    for (; !iterator.isDone(); iterator.next())
    {
        MObject currentNode = iterator.currentItem(&status);
        if (!status || currentNode.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        if (currentNode == rootObject || currentNode.hasFn(MFn::kDagNode))
        {
            continue;
        }

        MFnDependencyNode nodeFn(currentNode, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        const std::string nodeName = nodeFn.name().asChar();
        if (seenNames.insert(nodeName).second)
        {
            historyNames.append(nodeName.c_str());
        }
    }

    return MS::kSuccess;
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
