#include "DmxImportDeformers.h"
#include "DmxImportInternals.h"

#include <common/MayaCommandUtils.h>
#include <common/MayaDmxCommon.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MItDependencyGraph.h>

namespace dmx_import_impl
{

std::unordered_map<std::string, unsigned int> BuildInfluenceIndexByPath(const MDagPathArray &influencePaths)
{
    std::unordered_map<std::string, unsigned int> influenceByPath;
    for (unsigned int index = 0; index < influencePaths.length(); ++index)
    {
        influenceByPath[influencePaths[index].fullPathName().asChar()] = index;
    }
    return influenceByPath;
}

bool FindMatchingInfluencePath(
    const dmx_import_translator::ImportContext &context,
    const MDagPathArray &influencePaths,
    const MDagPath &requiredInfluencePath,
    MDagPath &matchedInfluencePath)
{
    const std::string requiredFullPath = requiredInfluencePath.fullPathName().asChar();
    const std::string requiredLeafName = requiredInfluencePath.partialPathName().asChar();
    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        const MDagPath &candidatePath = influencePaths[influenceIndex];
        if (requiredFullPath == candidatePath.fullPathName().asChar())
        {
            matchedInfluencePath = candidatePath;
            return true;
        }

        if (dcc_import_policy::MatchesNodeNameForAppend(
            context.scenePolicy,
            candidatePath.partialPathName().asChar(),
            requiredLeafName))
        {
            matchedInfluencePath = candidatePath;
            return true;
        }
    }

    return false;
}

MStatus EnsureSkinClusterContainsInfluences(
    const dmx_import_translator::ImportContext &context,
    const MObject &skinClusterObject,
    const MDagPathArray &requiredInfluencePaths,
    MDagPathArray &resolvedInfluencePaths)
{
    resolvedInfluencePaths.clear();
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    skinClusterFn.influenceObjects(resolvedInfluencePaths, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < requiredInfluencePaths.length(); ++influenceIndex)
    {
        MDagPath matchedInfluencePath;
        if (FindMatchingInfluencePath(context, resolvedInfluencePaths, requiredInfluencePaths[influenceIndex], matchedInfluencePath))
        {
            continue;
        }

        AppendImportDebugLog(
            (std::string("skinning: adding missing influence ")
                + requiredInfluencePaths[influenceIndex].fullPathName().asChar()).c_str());
        status = maya_cmd::AddSkinClusterInfluence(skinClusterNode.name(), requiredInfluencePaths[influenceIndex]);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    resolvedInfluencePaths.clear();
    skinClusterFn.influenceObjects(resolvedInfluencePaths, &status);
    return status ? MS::kSuccess : MS::kFailure;
}


DeformerImporter::DeformerImporter(std::shared_ptr<ImportContext> context)
    : context_(context)
{
}

MStatus DeformerImporter::bindMeshContext(const MObject &meshObject, const MObject &meshParentObject)
{
    meshObject_ = meshObject;
    meshParentObject_ = meshParentObject;

    MStatus status = MDagPath::getAPathTo(meshObject_, meshDagPath_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = MDagPath::getAPathTo(meshParentObject_, meshParentPath_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return MStatus::kSuccess;
}

MObject DeformerImporter::findPrimaryMeshChildForDeformers(const MObject &transformObject) const
{
    MStatus status;
    MFnDagNode dagNode(transformObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            continue;
        }

        MFnDagNode meshDagNode(childObject, &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            return childObject;
        }
    }

    return MObject::kNullObj;
}

MObject DeformerImporter::findExistingSkinClusterNode() const
{
    if (!dcc_import_policy::UsesUpdateCurrentScene(context_->scenePolicy) || meshObject_.isNull())
    {
        return MObject::kNullObj;
    }

    MStatus status;
    MObject rootObject = meshObject_;
    MItDependencyGraph iterator(
        rootObject,
        MFn::kSkinClusterFilter,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        MObject currentNode = iterator.currentItem(&status);
        if (!status || currentNode.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        return currentNode;
    }

    return MObject::kNullObj;
}

MStatus DeformerImporter::updateExistingSkinClusterBindings(
    const MObject &skinClusterObject,
    const MDagPathArray &influencePaths) const
{
    if (skinClusterObject.isNull())
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MDagPathArray existingInfluencePaths;
    status = EnsureSkinClusterContainsInfluences(*context_, skinClusterObject, influencePaths, existingInfluencePaths);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed while expanding existing skinCluster influences for ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    std::unordered_map<std::string, unsigned int> existingInfluenceByPath = BuildInfluenceIndexByPath(existingInfluencePaths);

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to bind dependency node for existing skinCluster on ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    const std::vector<std::string> bindPreMatrixStrings = FindAttributeStringArray(vertexData_, "mayaBindPreMatrix");
    const std::vector<std::string> influencePathStrings = FindAttributeStringArray(vertexData_, "mayaInfluencePaths");
    std::unordered_map<std::string, size_t> storedPathNameToIndex;
    for (size_t storedIndex = 0; storedIndex < influencePathStrings.size(); ++storedIndex)
    {
        const std::string &storedPath = influencePathStrings[storedIndex];
        const size_t lastSep = storedPath.rfind('|');
        const std::string jointName = (lastSep != std::string::npos) ? storedPath.substr(lastSep + 1) : storedPath;
        if (!jointName.empty())
        {
            storedPathNameToIndex[jointName] = storedIndex;
        }
    }

    MPlug bindPreMatrixArrayPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to find bindPreMatrix on existing skinCluster for ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        const std::string fullPath = influencePaths[influenceIndex].fullPathName().asChar();
        const auto existingIt = existingInfluenceByPath.find(fullPath);
        if (existingIt == existingInfluenceByPath.end())
        {
            return maya_dmx::ReportWarning(
                MString("maya_dmx: update skipped skinCluster overwrite because an existing influence did not match for ")
                + meshDagPath_.fullPathName());
        }

        MDagPath matchedInfluencePath;
        if (!FindMatchingInfluencePath(*context_, existingInfluencePaths, influencePaths[influenceIndex], matchedInfluencePath))
        {
            return maya_dmx::ReportWarning(
                MString("maya_dmx: update skipped skinCluster overwrite because a required influence path could not be resolved for ")
                + meshDagPath_.fullPathName());
        }

        const unsigned int logicalInfluenceIndex = skinClusterFn.indexForInfluenceObject(matchedInfluencePath, &status);
        if (!status)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to query logical influence index for existing skinCluster on ") + meshDagPath_.fullPathName());
            return MS::kFailure;
        }

        MPlug bindPreMatrixPlug = bindPreMatrixArrayPlug.elementByLogicalIndex(logicalInfluenceIndex, &status);
        if (!status)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to resolve bindPreMatrix element for existing skinCluster on ") + meshDagPath_.fullPathName());
            return MS::kFailure;
        }

        MMatrix bindPreMatrix = influencePaths[influenceIndex].inclusiveMatrixInverse();
        const size_t lastSep = fullPath.rfind('|');
        const std::string jointName = (lastSep != std::string::npos) ? fullPath.substr(lastSep + 1) : fullPath;
        auto storedIt = storedPathNameToIndex.find(jointName);
        if (storedIt != storedPathNameToIndex.end() && storedIt->second < bindPreMatrixStrings.size())
        {
            MMatrix parsedMatrix;
            if (ParseMatrixString(bindPreMatrixStrings[storedIt->second], parsedMatrix))
            {
                bindPreMatrix = parsedMatrix;
            }
        }

        MFnMatrixData matrixDataFn;
        MObject bindPreMatrixObject = matrixDataFn.create(bindPreMatrix, &status);
        if (!status)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to create bindPreMatrix value for existing skinCluster on ") + meshDagPath_.fullPathName());
            return MS::kFailure;
        }
        status = bindPreMatrixPlug.setMObject(bindPreMatrixObject);
        if (!status)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to assign bindPreMatrix value for existing skinCluster on ") + meshDagPath_.fullPathName());
            return MS::kFailure;
        }
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to find geomMatrix on existing skinCluster for ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    MMatrix geomMatrix = meshParentPath_.inclusiveMatrix();
    MMatrix parsedGeomMatrix;
    if (ParseMatrixString(FindAttributeString(vertexData_, "mayaGeomMatrix"), parsedGeomMatrix))
    {
        geomMatrix = parsedGeomMatrix;
    }

    MFnMatrixData geomMatrixDataFn;
    MObject geomMatrixObject = geomMatrixDataFn.create(geomMatrix, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to create geomMatrix value for existing skinCluster on ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    status = geomMatrixPlug.setMObject(geomMatrixObject);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to assign geomMatrix value for existing skinCluster on ") + meshDagPath_.fullPathName());
        return MS::kFailure;
    }

    return MS::kSuccess;
}

MStatus DeformerImporter::createSkinClusterWithApi(
    const MDagPathArray &influencePaths,
    MObject &skinClusterObject) const
{
    skinClusterObject = MObject::kNullObj;

    MStatus status;
    MFnMesh meshFn(meshDagPath_, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const MString originalShapeName = meshFn.name() + "Orig";
    MObject originalMeshObject = meshFn.copy(meshDagPath_.node(), meshParentPath_.node(), &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnDependencyNode originalMeshNode(originalMeshObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    originalMeshNode.setName(originalShapeName, false, &status);

    MPlug intermediatePlug = originalMeshNode.findPlug("intermediateObject", true, &status);
    if (status)
    {
        intermediatePlug.setBool(true);
    }

    MFnDependencyNode skinClusterNodeFn;
    skinClusterObject = skinClusterNodeFn.create("skinCluster", "mayaDmxSkinCluster#", &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const std::string requestedSkinClusterName = FindAttributeString(vertexData_, "mayaSkinClusterName");
    if (!requestedSkinClusterName.empty())
    {
        skinClusterNodeFn.setName(requestedSkinClusterName.c_str(), false, &status);
        status = MS::kSuccess;
    }

    MDGModifier dgModifier;
    auto connectArrayPlug = [&](const MObject &srcNode, const char *srcAttr, unsigned int srcIndex,
                                const MObject &dstNode, const char *dstAttr, unsigned int dstIndex) -> MStatus
    {
        MFnDependencyNode srcFn(srcNode);
        MFnDependencyNode dstFn(dstNode);
        MPlug srcPlug = srcFn.findPlug(srcAttr, true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        MPlug dstPlug = dstFn.findPlug(dstAttr, true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        if (srcPlug.isArray())
        {
            srcPlug = srcPlug.elementByLogicalIndex(srcIndex, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }
        if (dstPlug.isArray())
        {
            dstPlug = dstPlug.elementByLogicalIndex(dstIndex, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }
        }
        return dgModifier.connect(srcPlug, dstPlug);
    };

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug inputPlug = skinClusterNode.findPlug("input", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    inputPlug = inputPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MPlug inputGeometryPlug = inputPlug.child(0, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug sourceWorldMeshPlug = originalMeshNode.findPlug("worldMesh", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    sourceWorldMeshPlug = sourceWorldMeshPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = dgModifier.connect(sourceWorldMeshPlug, inputGeometryPlug);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = connectArrayPlug(originalMeshObject, "outMesh", 0, skinClusterObject, "originalGeometry", 0);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = connectArrayPlug(skinClusterObject, "outputGeometry", 0, meshDagPath_.node(), "inMesh", 0);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const std::vector<std::string> bindPreMatrixStrings = FindAttributeStringArray(vertexData_, "mayaBindPreMatrix");
    const std::vector<std::string> influencePathStrings = FindAttributeStringArray(vertexData_, "mayaInfluencePaths");

    std::unordered_map<std::string, size_t> storedPathNameToIndex;
    for (size_t storedIndex = 0; storedIndex < influencePathStrings.size(); ++storedIndex)
    {
        const std::string &storedPath = influencePathStrings[storedIndex];
        const size_t lastSep = storedPath.rfind('|');
        const std::string jointName = (lastSep != std::string::npos) ? storedPath.substr(lastSep + 1) : storedPath;
        if (!jointName.empty())
        {
            storedPathNameToIndex[jointName] = storedIndex;
        }
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        status = connectArrayPlug(influencePaths[influenceIndex].node(), "worldMatrix", 0, skinClusterObject, "matrix", influenceIndex);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MPlug bindPreMatrixPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        bindPreMatrixPlug = bindPreMatrixPlug.elementByLogicalIndex(influenceIndex, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }

        MFnMatrixData matrixDataFn;
        MMatrix bindPreMatrix = influencePaths[influenceIndex].inclusiveMatrixInverse();
        if (!bindPreMatrixStrings.empty())
        {
            const std::string fullPath = influencePaths[influenceIndex].fullPathName().asChar();
            const size_t lastSep = fullPath.rfind('|');
            const std::string jointName = (lastSep != std::string::npos) ? fullPath.substr(lastSep + 1) : fullPath;
            auto it = storedPathNameToIndex.find(jointName);
            if (it != storedPathNameToIndex.end() && it->second < bindPreMatrixStrings.size())
            {
                MMatrix parsedMatrix;
                if (ParseMatrixString(bindPreMatrixStrings[it->second], parsedMatrix))
                {
                    bindPreMatrix = parsedMatrix;
                }
            }
        }

        MObject bindPreMatrixObject = matrixDataFn.create(bindPreMatrix, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = bindPreMatrixPlug.setMObject(bindPreMatrixObject);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    MFnMatrixData geomMatrixDataFn;
    MMatrix geomMatrix = meshParentPath_.inclusiveMatrix();
    MMatrix parsedGeomMatrix;
    if (ParseMatrixString(FindAttributeString(vertexData_, "mayaGeomMatrix"), parsedGeomMatrix))
    {
        geomMatrix = parsedGeomMatrix;
    }
    MObject geomMatrixObject = geomMatrixDataFn.create(geomMatrix, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = geomMatrixPlug.setMObject(geomMatrixObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return dgModifier.doIt();
}

MStatus DeformerImporter::restoreSkinClusterSettings(const MObject &skinClusterObject) const
{
    MStatus status;
    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug skinningMethodPlug = skinClusterNode.findPlug("skinningMethod", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData_, "mayaSkinningMethod"));
        if (!values.empty())
        {
            skinningMethodPlug.setShort(static_cast<short>(values[0]));
        }
    }
    status = MS::kSuccess;

    MPlug useComponentsPlug = skinClusterNode.findPlug("useComponents", true, &status);
    if (status)
    {
        const std::string value = FindAttributeString(vertexData_, "mayaUseComponents");
        if (!value.empty())
        {
            useComponentsPlug.setBool(value == "1" || value == "true");
        }
    }
    status = MS::kSuccess;

    MPlug maxInfluencesPlug = skinClusterNode.findPlug("maxInfluences", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData_, "mayaMaxInfluences"));
        if (!values.empty())
        {
            maxInfluencesPlug.setInt(static_cast<int>(values[0]));
        }
    }
    status = MS::kSuccess;

    MPlug maintainMaxInfluencesPlug = skinClusterNode.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        const std::string value = FindAttributeString(vertexData_, "mayaMaintainMaxInfluences");
        if (!value.empty())
        {
            maintainMaxInfluencesPlug.setBool(value == "1" || value == "true");
        }
    }
    status = MS::kSuccess;

    MPlug normalizeWeightsPlug = skinClusterNode.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData_, "mayaNormalizeWeights"));
        if (!values.empty())
        {
            normalizeWeightsPlug.setShort(static_cast<short>(values[0]));
        }
    }

    return MS::kSuccess;
}

MObject DeformerImporter::findExistingBlendShapeNode(const std::string &blendShapeName) const
{
    if (!dcc_import_policy::UsesExistingObjectMerge(context_->scenePolicy) || blendShapeName.empty() || meshObject_.isNull())
    {
        return MObject::kNullObj;
    }

    MObject rootObject = meshObject_;
    MStatus status;
    MItDependencyGraph iterator(
        rootObject,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MObject::kNullObj;
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

        if (dcc_import_policy::MatchesNodeNameForAppend(context_->scenePolicy, dependencyNode.name().asChar(), blendShapeName))
        {
            return currentNode;
        }
    }

    return MObject::kNullObj;
}

DeformerImporter::ExistingBlendShapeInfo DeformerImporter::inspectExistingBlendShape(const MObject &blendShapeObject) const
{
    ExistingBlendShapeInfo info;
    if (blendShapeObject.isNull())
    {
        return info;
    }

    MStatus status;
    MFnDependencyNode dependencyNode(blendShapeObject, &status);
    if (!status)
    {
        return info;
    }

    info.node = blendShapeObject;
    info.nodeName = dependencyNode.name();

    MPlug weightArrayPlug = dependencyNode.findPlug("weight", true, &status);
    if (!status || weightArrayPlug.isNull())
    {
        return info;
    }

    unsigned int maxLogicalIndex = 0;
    bool hasAnyTarget = false;
    for (unsigned int elementIndex = 0; elementIndex < weightArrayPlug.numElements(); ++elementIndex)
    {
        MPlug weightElement = weightArrayPlug.elementByPhysicalIndex(elementIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        const unsigned int logicalIndex = weightElement.logicalIndex(&status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        hasAnyTarget = true;
        maxLogicalIndex = std::max(maxLogicalIndex, logicalIndex);
    }
    info.nextTargetIndex = hasAnyTarget ? (maxLogicalIndex + 1) : 0;

    MStringArray aliasPairs;
    status = maya_cmd::GetNodeAliasList(blendShapeObject, aliasPairs);
    if (!status)
    {
        return info;
    }

    for (unsigned int pairIndex = 0; pairIndex + 1 < aliasPairs.length(); pairIndex += 2)
    {
        const std::string aliasName = aliasPairs[pairIndex].asChar();
        const std::string plugName = aliasPairs[pairIndex + 1].asChar();
        const size_t leftBracket = plugName.find('[');
        const size_t rightBracket = plugName.find(']', leftBracket);
        if (leftBracket == std::string::npos || rightBracket == std::string::npos || rightBracket <= leftBracket + 1)
        {
            continue;
        }

        const unsigned int logicalIndex = static_cast<unsigned int>(std::strtoul(
            plugName.substr(leftBracket + 1, rightBracket - leftBracket - 1).c_str(),
            nullptr,
            10));
        info.targetIndicesByAlias[aliasName] = logicalIndex;
        info.nextTargetIndex = std::max(info.nextTargetIndex, logicalIndex + 1);
    }

    return info;
}

MStatus DeformerImporter::updateExistingBlendShapeTargetGeometry(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    const MPointArray &targetPoints) const
{
    MStringArray regenerateResult;
    MStatus status = maya_cmd::RegenerateBlendShapeTarget(blendShapeNodeName, weightIndex, regenerateResult);
    if (!status || regenerateResult.length() == 0)
    {
        return MS::kFailure;
    }

    const MString temporaryTransformName = regenerateResult[0];
    MSelectionList selection;
    status = selection.add(temporaryTransformName);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject temporaryTransformObject;
    status = selection.getDependNode(0, temporaryTransformObject);
    if (!status || temporaryTransformObject.isNull())
    {
        return MS::kFailure;
    }

    const MObject targetMeshObject = findPrimaryMeshChildForDeformers(temporaryTransformObject);
    if (targetMeshObject.isNull())
    {
        maya_cmd::DeleteNodeByName(temporaryTransformName);
        return MS::kFailure;
    }

    MFnMesh targetMeshFn(targetMeshObject, &status);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(temporaryTransformName);
        return MS::kFailure;
    }

    MPointArray editableTargetPoints(targetPoints);
    status = targetMeshFn.setPoints(editableTargetPoints, MSpace::kObject);
    maya_cmd::DeleteNodeByName(temporaryTransformName);
    return status ? MS::kSuccess : MS::kFailure;
}

void DeformerImporter::registerBlendShapeTargetBinding(
    const std::string &targetName,
    const dmx_import_translator::BlendShapeTargetBinding &binding)
{
    if (targetName.empty() || binding.node.isNull())
    {
        return;
    }

    std::vector<BlendShapeTargetBinding> &bindings = context_->importedBlendShapeTargets[targetName];
    for (const BlendShapeTargetBinding &existingBinding : bindings)
    {
        if (existingBinding.node == binding.node && existingBinding.weightIndex == binding.weightIndex)
        {
            return;
        }
    }

    bindings.push_back(binding);
}

MStatus DeformerImporter::applyBlendShapeAliases(
    const MFnDependencyNode &blendShapeDependency,
    const MString &blendShapeNodeName,
    const std::vector<std::pair<unsigned int, std::string>> &newAliasBindings) const
{
    MStatus weightPlugStatus;
    MPlug weightArrayPlug = blendShapeDependency.findPlug("weight", true, &weightPlugStatus);
    if (!weightPlugStatus || weightArrayPlug.isNull())
    {
        return MS::kSuccess;
    }

    for (const std::pair<unsigned int, std::string> &binding : newAliasBindings)
    {
        MStatus elementStatus;
        MPlug weightElement = weightArrayPlug.elementByLogicalIndex(binding.first, &elementStatus);
        if (!elementStatus)
        {
            continue;
        }

        maya_cmd::SetNodePlugAlias(blendShapeDependency.object(), weightElement, binding.second.c_str());
    }

    return MS::kSuccess;
}

MStatus DeformerImporter::ApplySkinning(
    const simple_dmx::Element *vertexData,
    const MObject &meshObject,
    const MObject &meshParentObject)
{
    vertexData_ = vertexData;
    MStatus status = bindMeshContext(meshObject, meshParentObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendImportDebugLog("skinning: begin");
    const std::vector<std::string> weightStrings = FindAttributeStringArray(vertexData_, "jointWeights");
    const std::vector<std::string> indexStrings = FindAttributeStringArray(vertexData_, "jointIndices");
    if (weightStrings.empty() || indexStrings.empty() || context_->jointOrder.empty())
    {
        return MS::kSuccess;
    }

    const std::vector<double> jointCountValues = ParseNumberList(FindAttributeString(vertexData_, "jointCount"));
    if (jointCountValues.empty())
    {
        return MS::kSuccess;
    }

    const int jointCount = static_cast<int>(jointCountValues[0]);
    if (jointCount <= 0)
    {
        return MS::kSuccess;
    }

    std::vector<float> jointWeights;
    for (const std::string &weightString : weightStrings)
    {
        const std::vector<double> values = ParseNumberList(weightString);
        if (!values.empty())
        {
            jointWeights.push_back(static_cast<float>(values[0]));
        }
    }

    std::vector<int> jointIndices;
    for (const std::string &indexString : indexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (!values.empty())
        {
            jointIndices.push_back(static_cast<int>(values[0]));
        }
    }

    if (jointWeights.empty() || jointIndices.empty() || jointWeights.size() != jointIndices.size())
    {
        return maya_dmx::ReportWarning("maya_dmx: skipped skinning because jointWeights/jointIndices were invalid.");
    }

    const size_t vertexCount = jointWeights.size() / static_cast<size_t>(jointCount);
    if (vertexCount == 0 || vertexCount * static_cast<size_t>(jointCount) != jointWeights.size())
    {
        return maya_dmx::ReportWarning("maya_dmx: skipped skinning because joint weight layout did not match jointCount.");
    }

    std::vector<bool> referencedJointMask(context_->jointOrder.size(), false);
    size_t skippedJointReferenceCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        const size_t baseOffset = static_cast<size_t>(vertexIndex) * static_cast<size_t>(jointCount);
        for (int slot = 0; slot < jointCount; ++slot)
        {
            const int dmxJointIndex = jointIndices[baseOffset + slot];
            if (dmxJointIndex < 0 || static_cast<size_t>(dmxJointIndex) >= context_->jointOrder.size())
            {
                ++skippedJointReferenceCount;
                continue;
            }

            referencedJointMask[static_cast<size_t>(dmxJointIndex)] = true;
        }
    }

    if (skippedJointReferenceCount > 0)
    {
        AppendImportDebugLog("skinning: skipped out-of-range joint indices while building influence list");
    }

    MDagPathArray activeInfluencePaths;
    std::vector<int> activeDmxJointIndices;
    for (size_t dmxJointIndex = 0; dmxJointIndex < context_->jointOrder.size(); ++dmxJointIndex)
    {
        if (!referencedJointMask[dmxJointIndex])
        {
            continue;
        }

        auto it = context_->importedDagPaths.find(context_->jointOrder[dmxJointIndex]);
        if (it == context_->importedDagPaths.end())
        {
            continue;
        }

        activeInfluencePaths.append(it->second);
        activeDmxJointIndices.push_back(static_cast<int>(dmxJointIndex));
    }

    if (activeInfluencePaths.length() == 0)
    {
        AppendImportDebugLog("skinning: no active joints");
        return MS::kSuccess;
    }

    MObject skinClusterObject = findExistingSkinClusterNode();
    const bool reusedExistingSkinCluster = !skinClusterObject.isNull();
    if (reusedExistingSkinCluster)
    {
        status = updateExistingSkinClusterBindings(skinClusterObject, activeInfluencePaths);
        if (!status)
        {
            return status;
        }

        status = maya_cmd::EnsureSkinClusterBindPose(activeInfluencePaths, meshParentPath_);
        if (!status)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to ensure bind pose for existing skinCluster on ") + meshDagPath_.fullPathName());
        }

        AppendImportDebugLog("skinning: reusing existing cluster");
    }
    else
    {
        status = createSkinClusterWithApi(activeInfluencePaths, skinClusterObject);
        if (!status || skinClusterObject.isNull())
        {
            return maya_dmx::ReportError(MString("maya_dmx: skinCluster API creation failed for ") + meshDagPath_.fullPathName(), status);
        }
        AppendImportDebugLog("skinning: created cluster");
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to bind MFnSkinCluster for ") + meshDagPath_.fullPathName());
        return MStatus::kFailure;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to create vertex component for ") + meshDagPath_.fullPathName());
        return MStatus::kFailure;
    }

    MIntArray vertexIds;
    for (int vertexIndex = 0; vertexIndex < static_cast<int>(vertexCount); ++vertexIndex)
    {
        vertexIds.append(vertexIndex);
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to populate vertex component for ") + meshDagPath_.fullPathName());
        return MStatus::kFailure;
    }

    MDagPathArray influencePaths;
    skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to query influence objects for ") + meshDagPath_.fullPathName());
        return MStatus::kFailure;
    }

    MIntArray influenceIndices;
    std::unordered_map<int, unsigned int> dmxJointToInfluenceSlot;
    for (unsigned int influencePathIndex = 0; influencePathIndex < activeInfluencePaths.length(); ++influencePathIndex)
    {
        MDagPath matchedInfluencePath;
        if (!FindMatchingInfluencePath(*context_, influencePaths, activeInfluencePaths[influencePathIndex], matchedInfluencePath))
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to match active influence path during update for ") + meshDagPath_.fullPathName());
            return MStatus::kFailure;
        }

        bool matchedInfluence = false;
        for (unsigned int clusterInfluenceIndex = 0; clusterInfluenceIndex < influencePaths.length(); ++clusterInfluenceIndex)
        {
            if (matchedInfluencePath.fullPathName() != influencePaths[clusterInfluenceIndex].fullPathName())
            {
                continue;
            }

            dmxJointToInfluenceSlot[activeDmxJointIndices[influencePathIndex]] = clusterInfluenceIndex;
            matchedInfluence = true;
            break;
        }

        if (!matchedInfluence)
        {
            maya_dmx::ReportWarning(MString("maya_dmx: failed to resolve matched influence slot during update for ") + meshDagPath_.fullPathName());
            return MStatus::kFailure;
        }
    }

    for (unsigned int influencePathIndex = 0; influencePathIndex < influencePaths.length(); ++influencePathIndex)
    {
        // setWeights expects the skinCluster's current physical influence order,
        // not the sparse logical matrix/bindPreMatrix index.
        influenceIndices.append(static_cast<int>(influencePathIndex));
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        maya_dmx::ReportWarning(MString("maya_dmx: failed to bind dependency node during weight write for ") + meshDagPath_.fullPathName());
        return MStatus::kFailure;
    }

    MPlug maintainMaxInfluencesPlug = skinClusterNode.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        maintainMaxInfluencesPlug.setBool(false);
    }
    status = MS::kSuccess;

    MPlug normalizeWeightsPlug = skinClusterNode.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        normalizeWeightsPlug.setShort(0);
    }
    status = MS::kSuccess;

    MPlug maxInfluencesPlug = skinClusterNode.findPlug("maxInfluences", true, &status);
    status = MS::kSuccess;

    MFloatArray weights;
    weights.setLength(static_cast<unsigned int>(vertexCount) * influenceIndices.length());
    for (unsigned int weightIndex = 0; weightIndex < weights.length(); ++weightIndex)
    {
        weights[weightIndex] = 0.0f;
    }

    unsigned int maxAssignedInfluences = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        const size_t baseOffset = static_cast<size_t>(vertexIndex) * static_cast<size_t>(jointCount);
        for (int slot = 0; slot < jointCount; ++slot)
        {
            const int dmxJointIndex = jointIndices[baseOffset + slot];
            const float weightValue = jointWeights[baseOffset + slot];
            auto influenceSlotIt = dmxJointToInfluenceSlot.find(dmxJointIndex);
            if (influenceSlotIt == dmxJointToInfluenceSlot.end())
            {
                continue;
            }

            const unsigned int influenceSlot = influenceSlotIt->second;
            if (weightValue > 0.0f)
            {
                weights[vertexIndex * influenceIndices.length() + influenceSlot] += weightValue;
            }
        }

        float totalWeight = 0.0f;
        unsigned int assignedInfluenceCount = 0;
        for (unsigned int influenceSlot = 0; influenceSlot < influenceIndices.length(); ++influenceSlot)
        {
            const float weightValue = weights[vertexIndex * influenceIndices.length() + influenceSlot];
            totalWeight += weightValue;
            if (weightValue > 1.0e-6f)
            {
                ++assignedInfluenceCount;
            }
        }
        maxAssignedInfluences = std::max(maxAssignedInfluences, assignedInfluenceCount);

        if (totalWeight > 1.0e-6f)
        {
            const float invTotalWeight = 1.0f / totalWeight;
            for (unsigned int influenceSlot = 0; influenceSlot < influenceIndices.length(); ++influenceSlot)
            {
                weights[vertexIndex * influenceIndices.length() + influenceSlot] *= invTotalWeight;
            }
        }
    }

    if (!maxInfluencesPlug.isNull())
    {
        maxInfluencesPlug.setInt(static_cast<int>(std::max(1u, maxAssignedInfluences)));
    }

    status = skinClusterFn.setWeights(meshDagPath_, vertexComponent, influenceIndices, weights, false);
    if (!status)
    {
        const MStatus setWeightsStatus = status;
        MFnDependencyNode failedSkinClusterNode(skinClusterObject, &status);
        const MString skinClusterName = status ? failedSkinClusterNode.name() : MString("<unknownSkinCluster>");

        maya_dmx::ReportWarning(
            MString("maya_dmx: setWeights failed during skin update for ")
            + meshDagPath_.fullPathName()
            + " (skinCluster="
            + skinClusterName
            + ", influences="
            + static_cast<int>(influenceIndices.length())
            + ", vertices="
            + vertexCount
            + ", error="
            + setWeightsStatus.errorString()
            + ")");
        return MStatus::kFailure;
    }

    AppendImportDebugLog("skinning: setWeights ok");
    return restoreSkinClusterSettings(skinClusterObject);
}

MStatus DeformerImporter::ApplyDeltaStates(
    const simple_dmx::Document &document,
    const simple_dmx::Element *meshElement,
    const MObject &meshObject,
    const MObject &meshParentObject,
    const MPointArray &basePoints)
{
    document_ = &document;
    meshElement_ = meshElement;
    basePoints_ = &basePoints;
    MStatus status = bindMeshContext(meshObject, meshParentObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendImportDebugLog("delta: begin");
    const std::vector<const simple_dmx::Element *> deltaStates = FindAttributeElementArray(*document_, meshElement_, "deltaStates");
    if (deltaStates.empty())
    {
        return MS::kSuccess;
    }

    std::vector<DeltaStateGroup> deltaStateGroups;
    std::unordered_map<std::string, size_t> deltaStateGroupIndex;
    for (const simple_dmx::Element *deltaState : deltaStates)
    {
        if (!deltaState)
        {
            continue;
        }

        std::string groupName = FindAttributeString(deltaState, "mayaBlendShapeNode");
        if (groupName.empty())
        {
            groupName = meshElement_->name.empty() ? std::string("dmx_blendShape") : meshElement_->name + "_blendShape";
        }

        auto [groupIt, inserted] = deltaStateGroupIndex.emplace(groupName, deltaStateGroups.size());
        if (inserted)
        {
            DeltaStateGroup group;
            group.nodeName = groupName;
            deltaStateGroups.push_back(std::move(group));
        }
        deltaStateGroups[groupIt->second].states.push_back(deltaState);
    }

    for (const DeltaStateGroup &group : deltaStateGroups)
    {
        const std::string blendShapeName = SanitizeNodeName(group.nodeName);
        ExistingBlendShapeInfo existingBlendShape = inspectExistingBlendShape(findExistingBlendShapeNode(blendShapeName));
        MStringArray targetTransforms;
        std::vector<MObject> targetMeshObjects;
        std::vector<std::pair<unsigned int, std::string>> newTargetBindings;
        for (const simple_dmx::Element *deltaState : group.states)
        {
            if (!deltaState)
            {
                continue;
            }

            const std::string targetName =
                deltaState->name.empty() ? std::string("delta") : SanitizeNodeName(deltaState->name);
            const std::vector<std::string> deltaPositionStrings = FindAttributeStringArray(deltaState, "positions");
            const std::vector<std::string> deltaPositionIndexStrings = FindAttributeStringArray(deltaState, "positionsIndices");
            if (deltaPositionStrings.empty() || deltaPositionIndexStrings.empty())
            {
                continue;
            }

            auto existingTargetIt = existingBlendShape.targetIndicesByAlias.find(targetName);
            MPointArray deltaPoints = *basePoints_;
            const size_t deltaCount = std::min(deltaPositionStrings.size(), deltaPositionIndexStrings.size());
            for (size_t i = 0; i < deltaCount; ++i)
            {
                const std::vector<double> deltaValues = ParseNumberList(deltaPositionStrings[i]);
                const std::vector<double> indexValues = ParseNumberList(deltaPositionIndexStrings[i]);
                if (deltaValues.size() < 3 || indexValues.empty())
                {
                    continue;
                }

                const int pointIndex = static_cast<int>(indexValues[0]);
                if (pointIndex < 0 || pointIndex >= static_cast<int>(deltaPoints.length()))
                {
                    continue;
                }

                deltaPoints[pointIndex].x += deltaValues[0];
                deltaPoints[pointIndex].y += deltaValues[1];
                deltaPoints[pointIndex].z += deltaValues[2];
            }

            if (existingTargetIt != existingBlendShape.targetIndicesByAlias.end())
            {
                if (dcc_import_policy::UsesUpdateCurrentScene(context_->scenePolicy))
                {
                    status = updateExistingBlendShapeTargetGeometry(
                        existingBlendShape.nodeName,
                        existingTargetIt->second,
                        deltaPoints);
                    if (!status)
                    {
                        return maya_dmx::ReportWarning(
                            MString("maya_dmx: update skipped blendShape target overwrite for ")
                            + targetName.c_str() + " on " + existingBlendShape.nodeName);
                    }
                }

                registerBlendShapeTargetBinding(
                    targetName,
                    BlendShapeTargetBinding{existingBlendShape.node, existingTargetIt->second});
                continue;
            }

            MObject duplicateTransformObject;
            MDagPath duplicateTransformPath;
            AppendImportDebugLog("duplicate base mesh target via API");
            status = maya_cmd::DuplicateDagNode(meshParentPath_, duplicateTransformObject, &duplicateTransformPath);
            if (!status || duplicateTransformObject.isNull())
            {
                return maya_dmx::ReportError(MString("maya_dmx: failed to duplicate base mesh for delta state ") + deltaState->name.c_str(), status);
            }

            const MObject duplicateMeshObject = findPrimaryMeshChildForDeformers(duplicateTransformObject);
            if (duplicateMeshObject.isNull())
            {
                return maya_dmx::ReportWarning(MString("maya_dmx: delta target duplicate had no mesh shape: ") + duplicateTransformPath.fullPathName());
            }

            MFnMesh targetMeshFn(duplicateMeshObject, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }

            status = targetMeshFn.setPoints(deltaPoints, MSpace::kObject);
            if (!status)
            {
                return MStatus::kFailure;
            }

            targetTransforms.append(duplicateTransformPath.fullPathName());
            targetMeshObjects.push_back(duplicateMeshObject);
            newTargetBindings.push_back({existingBlendShape.nextTargetIndex++, targetName});
        }

        if (targetTransforms.length() == 0 && existingBlendShape.node.isNull())
        {
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn;
        MObject blendShapeObject = existingBlendShape.node;
        MFnDependencyNode blendShapeDependency;
        MString blendShapeNodeName;
        const bool reusedExistingBlendShape = !blendShapeObject.isNull();
        if (!reusedExistingBlendShape)
        {
            blendShapeObject = blendShapeFn.create(meshObject_, MFnBlendShapeDeformer::kLocalOrigin, &status);
            if (!status)
            {
                return maya_dmx::ReportError(MString("maya_dmx: failed to create blendShape for ") + meshParentPath_.fullPathName(), status);
            }

            blendShapeDependency.setObject(blendShapeObject);
            if (blendShapeDependency.object().isNull())
            {
                return MStatus::kFailure;
            }
            blendShapeDependency.setName(blendShapeName.c_str(), false, &status);
            blendShapeNodeName = blendShapeDependency.name();
        }
        else
        {
            blendShapeFn.setObject(blendShapeObject);
            blendShapeDependency.setObject(blendShapeObject);
            if (blendShapeDependency.object().isNull())
            {
                return MStatus::kFailure;
            }
            blendShapeNodeName = blendShapeDependency.name();
        }

        if (!group.states.empty())
        {
            const simple_dmx::Element *metadataState = group.states.front();
            MPlug envelopePlug = blendShapeDependency.findPlug("envelope", true, &status);
            if (status)
            {
                const std::vector<double> values = ParseNumberList(FindAttributeString(metadataState, "mayaBlendShapeEnvelope"));
                if (!values.empty())
                {
                    envelopePlug.setFloat(static_cast<float>(values[0]));
                }
            }

            MPlug originPlug = blendShapeDependency.findPlug("origin", true, &status);
            if (status)
            {
                const std::vector<double> values = ParseNumberList(FindAttributeString(metadataState, "mayaBlendShapeOrigin"));
                if (!values.empty())
                {
                    originPlug.setShort(static_cast<short>(values[0]));
                }
            }
        }

        for (unsigned int targetIndex = 0; targetIndex < targetTransforms.length(); ++targetIndex)
        {
            status = blendShapeFn.addTarget(
                meshObject_,
                static_cast<int>(newTargetBindings[targetIndex].first),
                targetMeshObjects[targetIndex],
                1.0);
            if (!status)
            {
                return maya_dmx::ReportError(MString("maya_dmx: failed to add blendShape target to ") + blendShapeNodeName, status);
            }
        }

        for (unsigned int targetIndex = 0; targetIndex < targetTransforms.length(); ++targetIndex)
        {
            registerBlendShapeTargetBinding(
                newTargetBindings[targetIndex].second,
                BlendShapeTargetBinding{blendShapeObject, newTargetBindings[targetIndex].first});

            maya_cmd::DeleteNodeByName(targetTransforms[targetIndex]);
        }

        status = applyBlendShapeAliases(blendShapeDependency, blendShapeNodeName, newTargetBindings);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus ApplySkinning(
    const ImportContext &context,
    const simple_dmx::Element *vertexData,
    const MObject &meshObject,
    const MObject &meshParentObject)
{
    auto contextPtr = std::shared_ptr<ImportContext>(&const_cast<ImportContext &>(context), [](ImportContext *) {});
    DeformerImporter importer(contextPtr);
    return importer.ApplySkinning(vertexData, meshObject, meshParentObject);
}

MStatus ApplyDeltaStates(
    ImportContext &context,
    const simple_dmx::Document &document,
    const simple_dmx::Element *meshElement,
    const MObject &meshObject,
    const MObject &meshParentObject,
    const MPointArray &basePoints)
{
    auto contextPtr = std::shared_ptr<ImportContext>(&context, [](ImportContext *) {});
    DeformerImporter importer(contextPtr);
    return importer.ApplyDeltaStates(document, meshElement, meshObject, meshParentObject, basePoints);
}

} // namespace dmx_import_impl
