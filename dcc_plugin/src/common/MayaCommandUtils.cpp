#include "MayaCommandUtils.h"

#include <maya/MDGModifier.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>

#include <unordered_set>

namespace maya_cmd
{

namespace
{

MStatus ResolvePlugCommandNames(
    const MPlug &plug,
    MString &nodeName,
    MString &attributeName,
    MString &fullPlugName)
{
    if (plug.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    attributeName = plug.partialName(false, false, false, false, false, true, &status);
    if (!status || attributeName.length() == 0)
    {
        return MStatus::kFailure;
    }

    if (plug.node().hasFn(MFn::kDagNode))
    {
        MDagPath dagPath;
        status = MDagPath::getAPathTo(plug.node(), dagPath);
        if (!status)
        {
            return MStatus::kFailure;
        }
        nodeName = dagPath.fullPathName();
    }
    else
    {
        MFnDependencyNode nodeFn(plug.node(), &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        nodeName = nodeFn.name();
    }

    fullPlugName = nodeName;
    fullPlugName += ".";
    fullPlugName += attributeName;
    return MS::kSuccess;
}

} // namespace

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

MStatus EnsureShaderRegisteredInDefaultShaderList(const MObject &shaderObject)
{
    if (shaderObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnDependencyNode shaderFn(shaderObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    const MString classification = MFnDependencyNode::classification(shaderFn.typeName());

    if (classification.indexW("shader/surface") < 0)
    {
        return MS::kSuccess;
    }

    MObject defaultShaderListObject;
    if ((!TryGetNodeByName("defaultShaderList1", defaultShaderListObject) || defaultShaderListObject.isNull()) &&
        (!TryGetNodeByName(":defaultShaderList1", defaultShaderListObject) || defaultShaderListObject.isNull()))
    {
        return MS::kFailure;
    }

    MFnDependencyNode defaultShaderListFn(defaultShaderListObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MPlug shadersPlug = defaultShaderListFn.findPlug("shaders", true, &status);
    if (!status || !shadersPlug.isArray())
    {
        return MS::kFailure;
    }
    const MString shadersPlugName = shadersPlug.name();

    MPlug messagePlug = shaderFn.findPlug("message", true, &status);
    if (!status || messagePlug.isNull())
    {
        return MS::kFailure;
    }

    MPlugArray destinationConnections;
    messagePlug.connectedTo(destinationConnections, false, true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int connectionIndex = 0; connectionIndex < destinationConnections.length(); ++connectionIndex)
    {
        const MPlug destinationPlug = destinationConnections[connectionIndex];
        if (destinationPlug.isNull() || destinationPlug.node() != defaultShaderListObject)
        {
            continue;
        }

        const MPlug owningArrayPlug = destinationPlug.isElement() ? destinationPlug.array() :
            (destinationPlug.isChild() ? destinationPlug.parent() : destinationPlug);
        if (owningArrayPlug.name() == shadersPlugName)
        {
            return MS::kSuccess;
        }
    }

    unsigned int nextLogicalIndex = 0;
    const unsigned int existingElementCount = shadersPlug.evaluateNumElements();
    for (unsigned int elementIndex = 0; elementIndex < existingElementCount; ++elementIndex)
    {
        const MPlug existingElement = shadersPlug.elementByPhysicalIndex(elementIndex, &status);
        if (!status)
        {
            return MS::kFailure;
        }

        nextLogicalIndex = std::max(nextLogicalIndex, existingElement.logicalIndex() + 1);
    }

    MPlug targetElementPlug = shadersPlug.elementByLogicalIndex(nextLogicalIndex, &status);
    if (!status || targetElementPlug.isNull())
    {
        return MS::kFailure;
    }

    MDGModifier modifier;
    status = modifier.connect(messagePlug, targetElementPlug);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = modifier.doIt();
    if (status)
    {
        return MS::kSuccess;
    }

    destinationConnections.clear();
    messagePlug.connectedTo(destinationConnections, false, true, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int connectionIndex = 0; connectionIndex < destinationConnections.length(); ++connectionIndex)
    {
        const MPlug destinationPlug = destinationConnections[connectionIndex];
        if (destinationPlug.isNull() || destinationPlug.node() != defaultShaderListObject)
        {
            continue;
        }

        const MPlug owningArrayPlug = destinationPlug.isElement() ? destinationPlug.array() :
            (destinationPlug.isChild() ? destinationPlug.parent() : destinationPlug);
        if (owningArrayPlug.name() == shadersPlugName)
        {
            return MS::kSuccess;
        }
    }

    return MS::kFailure;
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

    if (dagPath.hasFn(MFn::kMesh))
    {
        MFnMesh meshFn(dagPath, &status);
        if (!status)
        {
            return MS::kFailure;
        }

        MObjectArray connectedShaders;
        MIntArray shaderIndices;
        status = meshFn.getConnectedShaders(dagPath.instanceNumber(), connectedShaders, shaderIndices);
        if (!status)
        {
            return MS::kFailure;
        }

        for (unsigned int shaderIndex = 0; shaderIndex < connectedShaders.length(); ++shaderIndex)
        {
            const MObject &connectedSetObject = connectedShaders[shaderIndex];
            if (connectedSetObject.isNull() || connectedSetObject == setObject || !connectedSetObject.hasFn(MFn::kSet))
            {
                continue;
            }

            MFnSet connectedSetFn(connectedSetObject, &status);
            if (!status)
            {
                return MS::kFailure;
            }

            if (connectedSetFn.restriction() != MFnSet::kRenderableOnly)
            {
                continue;
            }

            status = connectedSetFn.removeMember(dagPath.node());
            if (!status)
            {
                return MS::kFailure;
            }
        }
    }

    return setFn.addMember(dagPath);
}

MStatus AddComponentToSet(const MDagPath &dagPath, const MObject &componentObject, const MObject &setObject)
{
    if (componentObject.isNull())
    {
        return MStatus::kFailure;
    }

    MStatus status;
    MFnSet setFn(setObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    if (dagPath.hasFn(MFn::kMesh))
    {
        MFnMesh meshFn(dagPath, &status);
        if (!status)
        {
            return MS::kFailure;
        }

        MObjectArray connectedShaders;
        MIntArray shaderIndices;
        status = meshFn.getConnectedShaders(dagPath.instanceNumber(), connectedShaders, shaderIndices);
        if (!status)
        {
            return MS::kFailure;
        }

        for (unsigned int shaderIndex = 0; shaderIndex < connectedShaders.length(); ++shaderIndex)
        {
            const MObject &connectedSetObject = connectedShaders[shaderIndex];
            if (connectedSetObject.isNull() || connectedSetObject == setObject || !connectedSetObject.hasFn(MFn::kSet))
            {
                continue;
            }

            MFnSet connectedSetFn(connectedSetObject, &status);
            if (!status)
            {
                return MS::kFailure;
            }

            if (connectedSetFn.restriction() != MFnSet::kRenderableOnly)
            {
                continue;
            }

            status = connectedSetFn.removeMember(dagPath, componentObject);
            if (!status)
            {
                return MS::kFailure;
            }
        }
    }

    return setFn.addMember(dagPath, componentObject);
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

MStatus AddSkinClusterInfluence(
    const MString &skinClusterNodeName,
    const MDagPath &influencePath)
{
    if (skinClusterNodeName.length() == 0 || !influencePath.isValid())
    {
        return MS::kFailure;
    }

    // Keep the MEL command wrapped here for now. Maya does not expose a small
    // direct C++ helper for `skinCluster -e -addInfluence`, and the command keeps
    // the node's internal matrix/bind arrays consistent when expanding influences.
    MString command("skinCluster -e -ibp -lw true -wt 0.0 -ai \"");
    command += influencePath.fullPathName();
    command += "\" \"";
    command += skinClusterNodeName;
    command += "\"";
    return MGlobal::executeCommand(command, false, false);
}

MStatus EnsureSkinClusterBindPose(
    const MDagPathArray &influencePaths,
    const MDagPath &skinnedDagPath)
{
    if (!skinnedDagPath.isValid())
    {
        return MS::kFailure;
    }

    MStatus status;
    MObject skinnedNode = skinnedDagPath.node();
    MItDependencyGraph iterator(
        skinnedNode,
        MFn::kInvalid,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        MObject currentNode = iterator.currentItem(&status);
        if (!status || currentNode.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDependencyNode dependencyNode(currentNode, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (dependencyNode.typeName() == "dagPose")
        {
            return MS::kSuccess;
        }
    }

    MSelectionList previousSelection;
    MGlobal::getActiveSelectionList(previousSelection);

    MSelectionList bindPoseSelection;
    status = bindPoseSelection.add(skinnedDagPath);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        status = bindPoseSelection.add(influencePaths[influenceIndex]);
        if (!status)
        {
            MGlobal::setActiveSelectionList(previousSelection, MGlobal::kReplaceList);
            return MS::kFailure;
        }
    }

    status = MGlobal::setActiveSelectionList(bindPoseSelection, MGlobal::kReplaceList);
    if (!status)
    {
        MGlobal::setActiveSelectionList(previousSelection, MGlobal::kReplaceList);
        return MS::kFailure;
    }

    MStringArray result;
    status = MGlobal::executeCommand("dagPose -save -selection -bindPose", result, false, false);
    const MStatus restoreStatus = MGlobal::setActiveSelectionList(previousSelection, MGlobal::kReplaceList);
    if (!status)
    {
        return MS::kFailure;
    }

    return restoreStatus;
}

MStatus EnsureAnimationLayer(
    const MString &layerName,
    bool replaceExisting,
    bool overrideLayer,
    MString *resolvedLayerName)
{
    if (layerName.length() == 0)
    {
        return MS::kFailure;
    }

    MObject existingLayerObject;
    const bool layerExists = TryGetNodeByName(layerName, existingLayerObject) &&
        !existingLayerObject.isNull() &&
        existingLayerObject.hasFn(MFn::kDependencyNode);

    MStatus status = MS::kSuccess;
    if (layerExists)
    {
        MFnDependencyNode existingLayerFn(existingLayerObject, &status);
        if (!status || existingLayerFn.typeName() != "animLayer")
        {
            return MS::kFailure;
        }
    }

    if (layerExists && replaceExisting)
    {
        status = DeleteNodeByName(layerName);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    if (!layerExists || replaceExisting)
    {
        MString createCommand("animLayer \"");
        createCommand += layerName;
        createCommand += "\"";
        MString createdLayerName;
        status = MGlobal::executeCommand(createCommand, createdLayerName, false, false);
        if (!status || createdLayerName.length() == 0)
        {
            return MS::kFailure;
        }

        if (resolvedLayerName)
        {
            *resolvedLayerName = createdLayerName;
        }
        if (overrideLayer)
        {
            MString overrideCommand("animLayer -e -override true \"");
            overrideCommand += createdLayerName;
            overrideCommand += "\"";
            status = MGlobal::executeCommand(overrideCommand, false, false);
            if (!status)
            {
                return MS::kFailure;
            }
        }

        return MS::kSuccess;
    }

    MString resolvedName = layerName;
    if (overrideLayer)
    {
        MString overrideCommand("animLayer -e -override true \"");
        overrideCommand += resolvedName;
        overrideCommand += "\"";
        status = MGlobal::executeCommand(overrideCommand, false, false);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    if (resolvedLayerName)
    {
        *resolvedLayerName = resolvedName;
    }
    return MS::kSuccess;
}

MStatus AddPlugToAnimationLayer(
    const MString &layerName,
    const MPlug &plug)
{
    if (layerName.length() == 0 || plug.isNull())
    {
        return MS::kFailure;
    }

    MString nodeName;
    MString attributeName;
    MString fullPlugName;
    MStatus status = ResolvePlugCommandNames(plug, nodeName, attributeName, fullPlugName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MString command("animLayer -e -attribute \"");
    command += fullPlugName;
    command += "\" \"";
    command += layerName;
    command += "\"";
    return MGlobal::executeCommand(command, false, false);
}

MStatus ClearAnimationLayerCurve(
    const MString &layerName,
    const MPlug &plug)
{
    if (layerName.length() == 0 || plug.isNull())
    {
        return MS::kFailure;
    }

    MString nodeName;
    MString attributeName;
    MString fullPlugName;
    MStatus status = ResolvePlugCommandNames(plug, nodeName, attributeName, fullPlugName);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MString queryCommand("animLayer -q -findCurveForPlug \"");
    queryCommand += fullPlugName;
    queryCommand += "\" \"";
    queryCommand += layerName;
    queryCommand += "\"";

    MStringArray curveNames;
    status = MGlobal::executeCommand(queryCommand, curveNames, false, false);
    if (!status)
    {
        return MS::kSuccess;
    }

    for (unsigned int curveIndex = 0; curveIndex < curveNames.length(); ++curveIndex)
    {
        status = DeleteNodeByName(curveNames[curveIndex]);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SetKeyframesOnAnimationLayer(
    const MString &layerName,
    const MPlug &plug,
    const double *times,
    const double *values,
    size_t keyCount,
    bool timesAreSeconds)
{
    if (layerName.length() == 0 || plug.isNull() || !times || !values || keyCount == 0)
    {
        return MS::kFailure;
    }

    MStatus status = AddPlugToAnimationLayer(layerName, plug);
    if (!status)
    {
        return MS::kFailure;
    }

    status = ClearAnimationLayerCurve(layerName, plug);
    if (!status)
    {
        return MS::kFailure;
    }

    MString nodeName;
    MString attrName;
    MString fullPlugName;
    status = ResolvePlugCommandNames(plug, nodeName, attrName, fullPlugName);
    if (!status)
    {
        return MS::kFailure;
    }

    for (size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
    {
        MString command("setKeyframe -animLayer \"");
        command += layerName;
        command += "\" -attribute \"";
        command += attrName;
        command += "\" -time ";
        command += times[keyIndex];
        if (timesAreSeconds)
        {
            command += "sec";
        }
        command += " -value ";
        command += values[keyIndex];
        command += " \"";
        command += nodeName;
        command += "\"";
        status = MGlobal::executeCommand(command, false, false);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

} // namespace maya_cmd
