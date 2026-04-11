#include "DmxImportDeformers.h"
#include "DmxImportInternals.h"

#include "../common/MayaDmxCommon.h"

#include <algorithm>
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

namespace dmx_import_impl
{


DeformerImporter::DeformerImporter(ImportContext &context)
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
    originalMeshNode.setName(originalShapeName);

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
        skinClusterNodeFn.setName(requestedSkinClusterName.c_str(), &status);
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
    if (weightStrings.empty() || indexStrings.empty() || context_.jointOrder.empty())
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

    std::vector<bool> referencedJointMask(context_.jointOrder.size(), false);
    size_t skippedJointReferenceCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        const size_t baseOffset = static_cast<size_t>(vertexIndex) * static_cast<size_t>(jointCount);
        for (int slot = 0; slot < jointCount; ++slot)
        {
            const int dmxJointIndex = jointIndices[baseOffset + slot];
            if (dmxJointIndex < 0 || static_cast<size_t>(dmxJointIndex) >= context_.jointOrder.size())
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
    for (size_t dmxJointIndex = 0; dmxJointIndex < context_.jointOrder.size(); ++dmxJointIndex)
    {
        if (!referencedJointMask[dmxJointIndex])
        {
            continue;
        }

        auto it = context_.importedDagPaths.find(context_.jointOrder[dmxJointIndex]);
        if (it == context_.importedDagPaths.end())
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

    MObject skinClusterObject;
    status = createSkinClusterWithApi(activeInfluencePaths, skinClusterObject);
    if (!status || skinClusterObject.isNull())
    {
        return maya_dmx::ReportError(MString("maya_dmx: skinCluster API creation failed for ") + meshDagPath_.fullPathName(), status);
    }
    AppendImportDebugLog("skinning: created cluster");

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        vertexIds.append(vertexIndex);
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MIntArray influenceIndices;
    std::unordered_map<int, unsigned int> dmxJointToInfluenceSlot;
    for (unsigned int influencePathIndex = 0; influencePathIndex < activeInfluencePaths.length(); ++influencePathIndex)
    {
        const unsigned int influenceIndex = skinClusterFn.indexForInfluenceObject(activeInfluencePaths[influencePathIndex], &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        dmxJointToInfluenceSlot[activeDmxJointIndices[influencePathIndex]] = influenceIndices.length();
        influenceIndices.append(static_cast<int>(influenceIndex));
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
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
        MStringArray targetTransforms;
        std::vector<MObject> targetMeshObjects;
        std::vector<std::string> targetNames;
        for (const simple_dmx::Element *deltaState : group.states)
        {
            if (!deltaState)
            {
                continue;
            }

            const std::vector<std::string> deltaPositionStrings = FindAttributeStringArray(deltaState, "positions");
            const std::vector<std::string> deltaPositionIndexStrings = FindAttributeStringArray(deltaState, "positionsIndices");
            if (deltaPositionStrings.empty() || deltaPositionIndexStrings.empty())
            {
                continue;
            }

            MStringArray duplicateResult;
            MString duplicateCommand("duplicate -rr \"");
            duplicateCommand += meshParentPath_.fullPathName();
            duplicateCommand += "\"";
            AppendImportDebugLog(duplicateCommand.asChar());
            status = MGlobal::executeCommand(duplicateCommand, duplicateResult, false, false);
            if (!status || duplicateResult.length() == 0)
            {
                return maya_dmx::ReportError(MString("maya_dmx: failed to duplicate base mesh for delta state ") + deltaState->name.c_str(), status);
            }

            MSelectionList selectionList;
            selectionList.add(duplicateResult[0]);
            MObject duplicateTransformObject;
            status = selectionList.getDependNode(0, duplicateTransformObject);
            if (!status)
            {
                return MStatus::kFailure;
            }

            const MObject duplicateMeshObject = findPrimaryMeshChildForDeformers(duplicateTransformObject);
            if (duplicateMeshObject.isNull())
            {
                return maya_dmx::ReportWarning(MString("maya_dmx: delta target duplicate had no mesh shape: ") + duplicateResult[0]);
            }

            MFnMesh targetMeshFn(duplicateMeshObject, &status);
            if (!status)
            {
                return MStatus::kFailure;
            }

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

            status = targetMeshFn.setPoints(deltaPoints, MSpace::kObject);
            if (!status)
            {
                return MStatus::kFailure;
            }

            targetTransforms.append(duplicateResult[0]);
            targetMeshObjects.push_back(duplicateMeshObject);
            targetNames.push_back(deltaState->name.empty() ? std::string("delta") : SanitizeNodeName(deltaState->name));
        }

        if (targetTransforms.length() == 0)
        {
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn;
        const MObject blendShapeObject = blendShapeFn.create(meshObject_, MFnBlendShapeDeformer::kLocalOrigin, &status);
        if (!status)
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to create blendShape for ") + meshParentPath_.fullPathName(), status);
        }

        const std::string blendShapeName = SanitizeNodeName(group.nodeName);
        MFnDependencyNode blendShapeDependency(blendShapeObject, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        blendShapeDependency.setName(blendShapeName.c_str());
        const MString blendShapeNodeName = blendShapeDependency.name();

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
            status = blendShapeFn.addTarget(meshObject_, static_cast<int>(targetIndex), targetMeshObjects[targetIndex], 1.0);
            if (!status)
            {
                return maya_dmx::ReportError(MString("maya_dmx: failed to add blendShape target to ") + blendShapeNodeName, status);
            }
        }

        MStatus weightPlugStatus;
        MPlug weightArrayPlug = blendShapeDependency.findPlug("weight", true, &weightPlugStatus);
        const bool hasWeightPlug = weightPlugStatus && !weightArrayPlug.isNull();
        for (unsigned int targetIndex = 0; targetIndex < targetTransforms.length(); ++targetIndex)
        {
            context_.importedBlendShapeTargets[targetNames[targetIndex]].push_back(BlendShapeTargetBinding{blendShapeObject, targetIndex});

            MString deleteCommand("delete \"");
            deleteCommand += targetTransforms[targetIndex];
            deleteCommand += "\"";
            MGlobal::executeCommand(deleteCommand, false, false);

            if (hasWeightPlug)
            {
                MStatus elemStatus;
                MPlug weightElemPlug = weightArrayPlug.elementByLogicalIndex(targetIndex, &elemStatus);
                if (elemStatus)
                {
                    MString attrName = weightElemPlug.partialName();
                    MString aliasCmd("aliasAttr ");
                    aliasCmd += targetNames[targetIndex].c_str();
                    aliasCmd += " \"";
                    aliasCmd += blendShapeNodeName;
                    aliasCmd += ".";
                    aliasCmd += attrName;
                    aliasCmd += "\"";
                    MGlobal::executeCommand(aliasCmd, false, false);
                }
            }
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
    ImportContext &mutableContext = const_cast<ImportContext &>(context);
    DeformerImporter importer(mutableContext);
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
    DeformerImporter importer(context);
    return importer.ApplyDeltaStates(document, meshElement, meshObject, meshParentObject, basePoints);
}

} // namespace dmx_import_impl
