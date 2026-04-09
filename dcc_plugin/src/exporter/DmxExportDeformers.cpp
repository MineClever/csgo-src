#include "DmxExportDeformers.h"
#include "DmxExportInternals.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MDoubleArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MMatrix.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dmx_export_impl
{

void AppendSkinningData(const MDagPath &meshPath, Element &vertexDataElement, ExportContext &context)
{
    MStatus status;
    MObject meshNodeCopy(meshPath.node());
    MItDependencyGraph dgIt(meshNodeCopy, MFn::kSkinClusterFilter,
        MItDependencyGraph::kUpstream, MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel, &status);
    if (!status || dgIt.isDone())
    {
        return;
    }

    MObject skinClusterObject = dgIt.currentItem(&status);
    if (!status || skinClusterObject.isNull())
    {
        return;
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return;
    }

    MIntArray vertexIds;
    const unsigned int vertexCount = meshFn.numVertices(&status);
    if (!status || vertexCount == 0)
    {
        return;
    }

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }

    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return;
    }

    MDagPathArray influencePaths;
    const unsigned int influenceCount = skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status || influenceCount == 0)
    {
        return;
    }

    std::vector<int> influenceToJointIndex(influenceCount, -1);
    for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
    {
        const std::string pathKey = DagPathKey(influencePaths[influenceIndex]);
        auto it = context.jointIndexByPath.find(pathKey);
        if (it == context.jointIndexByPath.end())
        {
            auto dagIt = context.dagElementByPath.find(pathKey);
            if (dagIt != context.dagElementByPath.end() && dagIt->second)
            {
                const int jointIndex = static_cast<int>(context.jointElements.size());
                context.jointIndexByPath[pathKey] = jointIndex;
                context.jointElements.push_back(dagIt->second);
                it = context.jointIndexByPath.find(pathKey);
            }
        }
        if (it != context.jointIndexByPath.end())
        {
            influenceToJointIndex[influenceIndex] = it->second;
        }
    }

    MDoubleArray weights;
    unsigned int exportedInfluenceCount = 0;
    status = skinClusterFn.getWeights(meshPath, vertexComponent, weights, exportedInfluenceCount);
    if (!status || exportedInfluenceCount != influenceCount)
    {
        return;
    }

    constexpr double kWeightEpsilon = 1.0e-5;
    unsigned int jointCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        unsigned int activeCount = 0;
        const unsigned int baseOffset = vertexIndex * influenceCount;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            if (influenceToJointIndex[influenceIndex] < 0)
            {
                continue;
            }

            if (std::abs(weights[baseOffset + influenceIndex]) > kWeightEpsilon)
            {
                ++activeCount;
            }
        }

        jointCount = std::max(jointCount, activeCount);
    }

    if (jointCount == 0)
    {
        return;
    }

    std::vector<std::string> jointWeightValues;
    std::vector<std::string> jointIndexValues;
    jointWeightValues.reserve(static_cast<size_t>(vertexCount) * jointCount);
    jointIndexValues.reserve(static_cast<size_t>(vertexCount) * jointCount);

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const unsigned int baseOffset = vertexIndex * influenceCount;
        std::vector<std::pair<int, double>> vertexWeights;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            const int jointIndex = influenceToJointIndex[influenceIndex];
            const double weightValue = weights[baseOffset + influenceIndex];
            if (jointIndex < 0 || std::abs(weightValue) <= kWeightEpsilon)
            {
                continue;
            }

            vertexWeights.push_back({jointIndex, weightValue});
        }

        std::sort(vertexWeights.begin(), vertexWeights.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second != rhs.second)
            {
                return lhs.second > rhs.second;
            }
            return lhs.first < rhs.first;
        });

        if (vertexWeights.size() > jointCount)
        {
            vertexWeights.resize(jointCount);

            double weightSum = 0.0;
            for (const auto &entry : vertexWeights)
            {
                weightSum += entry.second;
            }

            if (weightSum > kWeightEpsilon)
            {
                for (auto &entry : vertexWeights)
                {
                    entry.second /= weightSum;
                }
            }
        }

        while (vertexWeights.size() < jointCount)
        {
            vertexWeights.push_back({-1, 0.0});
        }

        for (const auto &entry : vertexWeights)
        {
            jointIndexValues.push_back(std::to_string(entry.first));
            jointWeightValues.push_back(FormatFloat(entry.second));
        }
    }

    SetAttr(vertexDataElement, "jointCount", ScalarAttr("int", std::to_string(jointCount)));
    SetAttr(vertexDataElement, "jointIndices", ScalarArrayAttr("int_array", std::move(jointIndexValues)));
    SetAttr(vertexDataElement, "jointWeights", ScalarArrayAttr("float_array", std::move(jointWeightValues)));

    MFnDependencyNode skinClusterNodeFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    if (!context.exportMetadata)
    {
        return;
    }

    SetAttr(vertexDataElement, "mayaDeformerType", ScalarAttr("string", "skinCluster"));
    SetAttr(vertexDataElement, "mayaSkinClusterName", ScalarAttr("string", skinClusterNodeFn.name().asChar()));

    MPlug skinningMethodPlug = skinClusterNodeFn.findPlug("skinningMethod", true, &status);
    if (status)
    {
        short skinningMethod = 0;
        if (skinningMethodPlug.getValue(skinningMethod) == MS::kSuccess)
        {
            SetAttr(vertexDataElement, "mayaSkinningMethod", ScalarAttr("int", std::to_string(static_cast<int>(skinningMethod))));
        }
    }

    MPlug maxInfluencesPlug = skinClusterNodeFn.findPlug("maxInfluences", true, &status);
    if (status)
    {
        int maxInfluences = 0;
        if (maxInfluencesPlug.getValue(maxInfluences) == MS::kSuccess)
        {
            SetAttr(vertexDataElement, "mayaMaxInfluences", ScalarAttr("int", std::to_string(maxInfluences)));
        }
    }

    MPlug maintainMaxInfluencesPlug = skinClusterNodeFn.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        bool maintainMaxInfluences = false;
        if (maintainMaxInfluencesPlug.getValue(maintainMaxInfluences) == MS::kSuccess)
        {
            SetAttr(vertexDataElement, "mayaMaintainMaxInfluences", ScalarAttr("bool", maintainMaxInfluences ? "1" : "0"));
        }
    }

    MPlug normalizeWeightsPlug = skinClusterNodeFn.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        short normalizeWeights = 0;
        if (normalizeWeightsPlug.getValue(normalizeWeights) == MS::kSuccess)
        {
            SetAttr(vertexDataElement, "mayaNormalizeWeights", ScalarAttr("int", std::to_string(static_cast<int>(normalizeWeights))));
        }
    }

    MPlug useComponentsPlug = skinClusterNodeFn.findPlug("useComponents", true, &status);
    if (status)
    {
        bool useComponents = false;
        if (useComponentsPlug.getValue(useComponents) == MS::kSuccess)
        {
            SetAttr(vertexDataElement, "mayaUseComponents", ScalarAttr("bool", useComponents ? "1" : "0"));
        }
    }

    MPlug geomMatrixPlug = skinClusterNodeFn.findPlug("geomMatrix", true, &status);
    if (status)
    {
        const std::string geomMatrixValue = ReadMatrixPlugValue(geomMatrixPlug);
        if (!geomMatrixValue.empty())
        {
            SetAttr(vertexDataElement, "mayaGeomMatrix", ScalarAttr("string", geomMatrixValue));
        }
    }

    std::vector<std::string> bindPreMatrixValues;
    std::vector<std::string> influencePathValues;
    bindPreMatrixValues.reserve(influenceCount);
    influencePathValues.reserve(influenceCount);
    MPlug bindPreMatrixArrayPlug = skinClusterNodeFn.findPlug("bindPreMatrix", true, &status);
    if (status)
    {
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            influencePathValues.push_back(influencePaths[influenceIndex].fullPathName().asChar());

            MPlug bindPreMatrixPlug = bindPreMatrixArrayPlug.elementByLogicalIndex(influenceIndex, &status);
            if (!status)
            {
                bindPreMatrixValues.push_back(FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()));
                status = MS::kSuccess;
                continue;
            }

            const std::string bindPreMatrixValue = ReadMatrixPlugValue(bindPreMatrixPlug);
            bindPreMatrixValues.push_back(
                bindPreMatrixValue.empty() ?
                FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()) :
                bindPreMatrixValue);
        }
    }

    if (!influencePathValues.empty())
    {
        SetAttr(vertexDataElement, "mayaInfluencePaths", ScalarArrayAttr("string_array", std::move(influencePathValues)));
    }
    if (!bindPreMatrixValues.empty())
    {
        SetAttr(vertexDataElement, "mayaBindPreMatrix", ScalarArrayAttr("string_array", std::move(bindPreMatrixValues)));
    }
}

void AppendBlendShapeDeltaStates(
    DocumentBuilder &builder,
    const MDagPath &meshPath,
    const MPointArray &meshPoints,
    ExportContext &context,
    std::vector<Element *> &deltaStateElements)
{
    MStatus status;
    MObject meshNodeObject = meshPath.node();
    MItDependencyGraph dependencyIt(
        meshNodeObject,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return;
    }

    for (; !dependencyIt.isDone(); dependencyIt.next())
    {
        MObject blendShapeObject = dependencyIt.currentItem(&status);
        if (!status || blendShapeObject.isNull())
        {
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MIntArray weightIndices;
        status = blendShapeFn.weightIndexList(weightIndices);
        if (!status)
        {
            continue;
        }

        MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
        if (!status)
        {
            continue;
        }

        for (unsigned int weightSlot = 0; weightSlot < weightIndices.length(); ++weightSlot)
        {
            const unsigned int weightIndex = static_cast<unsigned int>(weightIndices[weightSlot]);

            // Read the weight alias before calling TryRegenerateBlendShapeTarget,
            // because sculptTarget -regenerate may rename it to the shape node name.
            MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(weightIndex, &status);
            MString preSculptAlias;
            if (status && !weightPlug.isNull())
            {
                preSculptAlias = blendShapeNodeFn.plugsAlias(weightPlug);
            }
            status = MS::kSuccess;

            MObjectArray targets;
            status = blendShapeFn.getTargets(meshNodeObject, static_cast<int>(weightIndex), targets);
            MDagPath targetPath;
            MString temporaryTargetTransform;
            MString targetNodeName;
            if (status && targets.length() > 0)
            {
                if (!TryGetMeshPathFromObject(targets[0], targetPath))
                {
                    continue;
                }

                MFnDagNode targetDagNode(targetPath, &status);
                if (!status)
                {
                    continue;
                }
                targetNodeName = targetDagNode.name();
            }
            else
            {
                if (!TryRegenerateBlendShapeTarget(blendShapeNodeFn.name(), weightIndex, targetPath, temporaryTargetTransform))
                {
                    continue;
                }

                // Restore the original weight alias if sculptTarget renamed it.
                if (preSculptAlias.length() > 0 && !weightPlug.isNull())
                {
                    MString currentAlias = blendShapeNodeFn.plugsAlias(weightPlug);
                    if (currentAlias != preSculptAlias)
                    {
                        MString attrName = weightPlug.partialName();
                        blendShapeNodeFn.setAlias(preSculptAlias, attrName, weightPlug, true);
                    }
                }

                MFnDagNode targetDagNode(targetPath, &status);
                if (!status)
                {
                    if (temporaryTargetTransform.length() > 0)
                    {
                        MString deleteCommand("delete \"");
                        deleteCommand += temporaryTargetTransform;
                        deleteCommand += "\"";
                        MGlobal::executeCommand(deleteCommand, false, false);
                    }
                    continue;
                }
                targetNodeName = targetDagNode.name();
            }

            MFnMesh targetMeshFn(targetPath, &status);
            if (!status)
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MSelectionList deleteList;
                    MObject deleteObject;
                    if (deleteList.add(temporaryTargetTransform) == MS::kSuccess &&
                        deleteList.getDependNode(0, deleteObject) == MS::kSuccess)
                    {
                        MDGModifier dgModifier;
                        dgModifier.deleteNode(deleteObject);
                        dgModifier.doIt();
                    }
                }
                continue;
            }

            MPointArray targetPoints;
            status = targetMeshFn.getPoints(targetPoints, MSpace::kObject);
            if (!status || targetPoints.length() != meshPoints.length())
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MSelectionList deleteList;
                    MObject deleteObject;
                    if (deleteList.add(temporaryTargetTransform) == MS::kSuccess &&
                        deleteList.getDependNode(0, deleteObject) == MS::kSuccess)
                    {
                        MDGModifier dgModifier;
                        dgModifier.deleteNode(deleteObject);
                        dgModifier.doIt();
                    }
                }
                continue;
            }

            std::vector<std::string> deltaPositions;
            std::vector<std::string> deltaPositionIndices;
            deltaPositions.reserve(targetPoints.length());
            deltaPositionIndices.reserve(targetPoints.length());
            for (unsigned int pointIndex = 0; pointIndex < targetPoints.length(); ++pointIndex)
            {
                const double dx = targetPoints[pointIndex].x - meshPoints[pointIndex].x;
                const double dy = targetPoints[pointIndex].y - meshPoints[pointIndex].y;
                const double dz = targetPoints[pointIndex].z - meshPoints[pointIndex].z;
                if (std::abs(dx) < 1.0e-6 && std::abs(dy) < 1.0e-6 && std::abs(dz) < 1.0e-6)
                {
                    continue;
                }

                deltaPositions.push_back(FormatVector3(dx, dy, dz));
                deltaPositionIndices.push_back(std::to_string(pointIndex));
            }

            if (deltaPositions.empty())
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MSelectionList deleteList;
                    MObject deleteObject;
                    if (deleteList.add(temporaryTargetTransform) == MS::kSuccess &&
                        deleteList.getDependNode(0, deleteObject) == MS::kSuccess)
                    {
                        MDGModifier dgModifier;
                        dgModifier.deleteNode(deleteObject);
                        dgModifier.doIt();
                    }
                }
                continue;
            }

            std::string deltaName = targetNodeName.asChar();
            if (preSculptAlias.length() > 0)
            {
                deltaName = preSculptAlias.asChar();
            }

            Element *deltaElement = builder.CreateElement("DmeVertexDeltaData", deltaName);
            SetAttr(*deltaElement, "vertexFormat", ScalarArrayAttr("string_array", {"positions"}));
            SetAttr(*deltaElement, "positions", ScalarArrayAttr("vector3_array", std::move(deltaPositions)));
            SetAttr(*deltaElement, "positionsIndices", ScalarArrayAttr("int_array", std::move(deltaPositionIndices)));
            if (context.exportMetadata)
            {
                SetAttr(*deltaElement, "mayaDeformerType", ScalarAttr("string", "blendShape"));
                SetAttr(*deltaElement, "mayaBlendShapeNode", ScalarAttr("string", blendShapeNodeFn.name().asChar()));
                SetAttr(*deltaElement, "mayaWeightIndex", ScalarAttr("int", std::to_string(weightIndex)));
                SetAttr(*deltaElement, "mayaTargetName", ScalarAttr("string", targetNodeName.asChar()));
                MPlug envelopePlug = blendShapeNodeFn.findPlug("envelope", true, &status);
                if (status)
                {
                    float envelope = 1.0f;
                    if (envelopePlug.getValue(envelope) == MS::kSuccess)
                    {
                        SetAttr(*deltaElement, "mayaBlendShapeEnvelope", ScalarAttr("float", FormatFloat(envelope)));
                    }
                }
                MPlug originPlug = blendShapeNodeFn.findPlug("origin", true, &status);
                if (status)
                {
                    short origin = 0;
                    if (originPlug.getValue(origin) == MS::kSuccess)
                    {
                        SetAttr(*deltaElement, "mayaBlendShapeOrigin", ScalarAttr("int", std::to_string(static_cast<int>(origin))));
                    }
                }
            }

            if (temporaryTargetTransform.length() > 0)
            {
                MString deleteCommand("delete \"");
                deleteCommand += temporaryTargetTransform;
                deleteCommand += "\"";
                MGlobal::executeCommand(deleteCommand, false, false);
            }

            deltaStateElements.push_back(deltaElement);
        }
    }
}

} // namespace dmx_export_impl
