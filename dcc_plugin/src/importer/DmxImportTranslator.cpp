#include "DmxImportTranslator.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxText.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Windows.h>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDagModifier.h>
#include <maya/MDGModifier.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFloatArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVectorArray.h>
#include <maya/MVector.h>

namespace
{
void AppendImportDebugLog(const char *message)
{
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }

    std::string logPath(tempPath);
    logPath += "maya_dmx_import_debug.log";

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        return;
    }

    logFile << message << "\n";
}

struct FaceSetAssignment
{
    std::string shadingGroupName;
    int polygonStart = 0;
    int polygonCount = 0;
};

struct ImportContext
{
    const simple_dmx::Document &document;
    const simple_dmx::Element *modelRoot = nullptr;
    std::vector<const simple_dmx::Element *> jointOrder;
    std::unordered_set<const simple_dmx::Element *> jointSet;
    std::unordered_map<const simple_dmx::Element *, MDagPath> importedDagPaths;
};

std::string ReadTextFile(const MFileObject &fileObject)
{
    std::ifstream file(fileObject.rawFullName().asChar(), std::ios::in | std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::vector<double> ParseNumberList(const std::string &text)
{
    std::string normalized = text;
    std::replace_if(
        normalized.begin(),
        normalized.end(),
        [](char c)
        {
            return c == ',' || c == '(' || c == ')' || c == '[' || c == ']';
        },
        ' ');

    std::istringstream stream(normalized);
    std::vector<double> values;
    double value = 0.0;
    while (stream >> value)
    {
        values.push_back(value);
    }

    return values;
}

const simple_dmx::Element *FindAttributeElement(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return nullptr;
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end())
    {
        return nullptr;
    }

    return document.ResolveElement(it->second);
}

std::vector<const simple_dmx::Element *> FindAttributeElementArray(const simple_dmx::Document &document, const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end())
    {
        return {};
    }

    return document.ResolveElementArray(it->second);
}

std::string FindAttributeString(const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != simple_dmx::Attribute::Kind::String)
    {
        return {};
    }

    return it->second.stringValue;
}

std::vector<std::string> FindAttributeStringArray(const simple_dmx::Element *element, const char *attributeName)
{
    if (!element)
    {
        return {};
    }

    auto it = element->attributes.find(attributeName);
    if (it == element->attributes.end() || it->second.kind != simple_dmx::Attribute::Kind::StringArray)
    {
        return {};
    }

    return it->second.stringArray;
}

MStatus ApplyTransform(const simple_dmx::Document &document, const simple_dmx::Element *dagElement, MObject object)
{
    const simple_dmx::Element *transformElement = FindAttributeElement(document, dagElement, "transform");
    if (!transformElement)
    {
        return MS::kSuccess;
    }

    const std::vector<double> positionValues = ParseNumberList(FindAttributeString(transformElement, "position"));
    const std::vector<double> orientationValues = ParseNumberList(FindAttributeString(transformElement, "orientation"));

    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return status;
    }

    if (positionValues.size() >= 3)
    {
        status = transformFn.setTranslation(MVector(positionValues[0], positionValues[1], positionValues[2]), MSpace::kTransform);
        if (!status)
        {
            return status;
        }
    }

    if (orientationValues.size() >= 4)
    {
        status = transformFn.setRotation(MQuaternion(
            orientationValues[0],
            orientationValues[1],
            orientationValues[2],
            orientationValues[3]));
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

MObject CreateDagNode(const std::string &name, bool isJoint, MObject parent, MStatus &status)
{
    if (isJoint)
    {
        MFnIkJoint jointFn;
        MObject jointObject = jointFn.create(parent, &status);
        if (status)
        {
            jointFn.setName(name.c_str());
        }
        return jointObject;
    }

    MFnTransform transformFn;
    MObject transformObject = transformFn.create(parent, &status);
    if (status)
    {
        transformFn.setName(name.c_str());
    }
    return transformObject;
}

void CollectJointInfo(
    const simple_dmx::Document &document,
    const simple_dmx::Element *modelElement,
    ImportContext &context)
{
    for (const simple_dmx::Element *joint : FindAttributeElementArray(document, modelElement, "jointList"))
    {
        if (joint)
        {
            context.jointOrder.push_back(joint);
            context.jointSet.insert(joint);
        }
    }
}

const simple_dmx::Element *FindMeshVertexData(const simple_dmx::Document &document, const simple_dmx::Element *meshElement)
{
    if (const simple_dmx::Element *bindState = FindAttributeElement(document, meshElement, "bindState"))
    {
        return bindState;
    }

    for (const simple_dmx::Element *baseState : FindAttributeElementArray(document, meshElement, "baseStates"))
    {
        if (baseState && (baseState->name == "bind" || baseState->name == "Bind"))
        {
            return baseState;
        }
    }

    const std::vector<const simple_dmx::Element *> baseStates = FindAttributeElementArray(document, meshElement, "baseStates");
    return baseStates.empty() ? nullptr : baseStates.front();
}

MObject FindSkinClusterForMesh(const MObject &meshObject);
MStatus CreateSkinClusterWithApi(const MDagPathArray &influencePaths, const MDagPath &meshDagPath, const MDagPath &meshParentPath, MObject &skinClusterObject);

MStatus ApplySkinning(const ImportContext &context, const simple_dmx::Element *vertexData, const MObject &meshObject, const MObject &meshParentObject)
{
    AppendImportDebugLog("skinning: begin");
    const std::vector<std::string> weightStrings = FindAttributeStringArray(vertexData, "jointWeights");
    const std::vector<std::string> indexStrings = FindAttributeStringArray(vertexData, "jointIndices");
    if (weightStrings.empty() || indexStrings.empty() || context.jointOrder.empty())
    {
        return MS::kSuccess;
    }

    const std::vector<double> jointCountValues = ParseNumberList(FindAttributeString(vertexData, "jointCount"));
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
    jointWeights.reserve(weightStrings.size());
    for (const std::string &weightString : weightStrings)
    {
        const std::vector<double> values = ParseNumberList(weightString);
        if (!values.empty())
        {
            jointWeights.push_back(static_cast<float>(values[0]));
        }
    }

    std::vector<int> jointIndices;
    jointIndices.reserve(indexStrings.size());
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

    MStatus status;
    MDagPathArray activeInfluencePaths;
    for (const simple_dmx::Element *jointElement : context.jointOrder)
    {
        auto it = context.importedDagPaths.find(jointElement);
        if (it == context.importedDagPaths.end())
        {
            continue;
        }

        activeInfluencePaths.append(it->second);
    }

    if (activeInfluencePaths.length() == 0)
    {
        AppendImportDebugLog("skinning: no active joints");
        return MS::kSuccess;
    }

    MDagPath meshParentPath;
    status = MDagPath::getAPathTo(meshParentObject, meshParentPath);
    if (!status)
    {
        return status;
    }

    MDagPath meshDagPath;
    status = MDagPath::getAPathTo(meshObject, meshDagPath);
    if (!status)
    {
        return status;
    }

    MObject skinClusterObject;
    status = CreateSkinClusterWithApi(activeInfluencePaths, meshDagPath, meshParentPath, skinClusterObject);
    if (!status || skinClusterObject.isNull())
    {
        return maya_dmx::ReportError(MString("maya_dmx: skinCluster API creation failed for ") + meshDagPath.fullPathName(), status);
    }
    AppendImportDebugLog("skinning: created cluster");

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return status;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return status;
    }

    MIntArray vertexIds;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        vertexIds.append(vertexIndex);
    }
    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return status;
    }

    MIntArray influenceIndices;
    std::unordered_map<int, unsigned int> dmxJointToInfluenceSlot;
    for (unsigned int dmxJointIndex = 0; dmxJointIndex < activeInfluencePaths.length(); ++dmxJointIndex)
    {
        const unsigned int influenceIndex = skinClusterFn.indexForInfluenceObject(activeInfluencePaths[dmxJointIndex], &status);
        if (!status)
        {
            return status;
        }
        dmxJointToInfluenceSlot[static_cast<int>(dmxJointIndex)] = influenceIndices.length();
        influenceIndices.append(static_cast<int>(influenceIndex));
    }

    if (influenceIndices.length() == 0)
    {
        return MS::kSuccess;
    }

    MFloatArray weights;
    weights.setLength(static_cast<unsigned int>(vertexCount) * influenceIndices.length());
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
            weights[vertexIndex * influenceIndices.length() + influenceSlot] = weightValue;
        }
    }

    status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
    if (status)
    {
        AppendImportDebugLog("skinning: setWeights ok");
    }
    return status;
}

std::string SanitizeNodeName(const std::string &name)
{
    std::string sanitized = name.empty() ? "dmxMaterial" : name;
    for (char &ch : sanitized)
    {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_';
        if (!ok)
        {
            ch = '_';
        }
    }
    return sanitized;
}

MStatus AssignFaceSetMaterials(
    const MFnMesh &meshFn,
    const std::vector<FaceSetAssignment> &faceSetAssignments)
{
    if (faceSetAssignments.empty())
    {
        return MS::kSuccess;
    }

    const MString meshPath = meshFn.fullPathName();
    for (const FaceSetAssignment &assignment : faceSetAssignments)
    {
        if (assignment.polygonCount <= 0 || assignment.shadingGroupName.empty())
        {
            continue;
        }

        const std::string sanitizedName = SanitizeNodeName(assignment.shadingGroupName);
        const std::string shaderName = sanitizedName + "_shader";

        MString command;
        command += "if (!`objExists \"";
        command += assignment.shadingGroupName.c_str();
        command += "\"`) {";
        command += "string $shader = \"";
        command += shaderName.c_str();
        command += "\";";
        command += "if (!`objExists $shader`) $shader = `shadingNode -asShader lambert -name $shader`;";
        command += "string $sg = `sets -renderable true -noSurfaceShader true -empty -name \"";
        command += assignment.shadingGroupName.c_str();
        command += "\"`;";
        command += "connectAttr -f ($shader + \".outColor\") ($sg + \".surfaceShader\");";
        command += "}";
        if (MGlobal::executeCommand(command, false, false) != MS::kSuccess)
        {
            continue;
        }

        MString components;
        for (int offset = 0; offset < assignment.polygonCount; ++offset)
        {
            components += " \"";
            components += meshPath;
            components += ".f[";
            components += assignment.polygonStart + offset;
            components += "]\"";
        }

        MString assignCommand("sets -e -forceElement \"");
        assignCommand += assignment.shadingGroupName.c_str();
        assignCommand += "\"";
        assignCommand += components;
        MGlobal::executeCommand(assignCommand, false, false);
    }

    return MS::kSuccess;
}

MObject FindPrimaryMeshChild(const MObject &transformObject)
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

MObject FindSkinClusterForMesh(const MObject &meshObject)
{
    MStatus status;
    MObject meshObjectCopy(meshObject);
    MItDependencyGraph iterator(
        meshObjectCopy,
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
        MObject current = iterator.currentItem(&status);
        if (status && !current.isNull() && current.hasFn(MFn::kSkinClusterFilter))
        {
            return current;
        }
    }

    return MObject::kNullObj;
}

MStatus CreateSkinClusterWithApi(const MDagPathArray &influencePaths, const MDagPath &meshDagPath, const MDagPath &meshParentPath, MObject &skinClusterObject)
{
    skinClusterObject = MObject::kNullObj;

    MStatus status;
    MFnMesh meshFn(meshDagPath, &status);
    if (!status)
    {
        return status;
    }

    const MString originalShapeName = meshFn.name() + "Orig";
    MObject originalMeshObject = meshFn.copy(meshDagPath.node(), meshParentPath.node(), &status);
    if (!status)
    {
        return status;
    }

    MFnDependencyNode originalMeshNode(originalMeshObject, &status);
    if (!status)
    {
        return status;
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
        return status;
    }

    MDGModifier dgModifier;
    const auto connectArrayPlug = [&](const MObject &srcNode, const char *srcAttr, unsigned int srcIndex,
                                      const MObject &dstNode, const char *dstAttr, unsigned int dstIndex) -> MStatus
    {
        MFnDependencyNode srcFn(srcNode);
        MFnDependencyNode dstFn(dstNode);
        MPlug srcPlug = srcFn.findPlug(srcAttr, true, &status);
        if (!status)
        {
            return status;
        }
        MPlug dstPlug = dstFn.findPlug(dstAttr, true, &status);
        if (!status)
        {
            return status;
        }
        if (srcPlug.isArray())
        {
            srcPlug = srcPlug.elementByLogicalIndex(srcIndex, &status);
            if (!status)
            {
                return status;
            }
        }
        if (dstPlug.isArray())
        {
            dstPlug = dstPlug.elementByLogicalIndex(dstIndex, &status);
            if (!status)
            {
                return status;
            }
        }
        return dgModifier.connect(srcPlug, dstPlug);
    };

    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return status;
    }
    MPlug inputPlug = skinClusterNode.findPlug("input", true, &status);
    if (!status)
    {
        return status;
    }
    inputPlug = inputPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return status;
    }
    MPlug inputGeometryPlug = inputPlug.child(0, &status);
    if (!status)
    {
        return status;
    }

    MPlug sourceWorldMeshPlug = originalMeshNode.findPlug("worldMesh", true, &status);
    if (!status)
    {
        return status;
    }
    sourceWorldMeshPlug = sourceWorldMeshPlug.elementByLogicalIndex(0, &status);
    if (!status)
    {
        return status;
    }
    status = dgModifier.connect(sourceWorldMeshPlug, inputGeometryPlug);
    if (!status)
    {
        return status;
    }

    status = connectArrayPlug(originalMeshObject, "outMesh", 0, skinClusterObject, "originalGeometry", 0);
    if (!status)
    {
        return status;
    }
    status = connectArrayPlug(skinClusterObject, "outputGeometry", 0, meshDagPath.node(), "inMesh", 0);
    if (!status)
    {
        return status;
    }

    for (unsigned int influenceIndex = 0; influenceIndex < influencePaths.length(); ++influenceIndex)
    {
        status = connectArrayPlug(influencePaths[influenceIndex].node(), "worldMatrix", 0, skinClusterObject, "matrix", influenceIndex);
        if (!status)
        {
            return status;
        }

        MPlug bindPreMatrixPlug = skinClusterNode.findPlug("bindPreMatrix", true, &status);
        if (!status)
        {
            return status;
        }
        bindPreMatrixPlug = bindPreMatrixPlug.elementByLogicalIndex(influenceIndex, &status);
        if (!status)
        {
            return status;
        }

        MFnMatrixData matrixDataFn;
        MObject bindPreMatrixObject = matrixDataFn.create(influencePaths[influenceIndex].inclusiveMatrixInverse(), &status);
        if (!status)
        {
            return status;
        }
        status = bindPreMatrixPlug.setMObject(bindPreMatrixObject);
        if (!status)
        {
            return status;
        }
    }

    MPlug geomMatrixPlug = skinClusterNode.findPlug("geomMatrix", true, &status);
    if (!status)
    {
        return status;
    }
    MFnMatrixData geomMatrixDataFn;
    MObject geomMatrixObject = geomMatrixDataFn.create(meshParentPath.inclusiveMatrix(), &status);
    if (!status)
    {
        return status;
    }
    status = geomMatrixPlug.setMObject(geomMatrixObject);
    if (!status)
    {
        return status;
    }

    return dgModifier.doIt();
}

MStatus ApplyDeltaStates(
    const simple_dmx::Document &document,
    const simple_dmx::Element *meshElement,
    const MObject &meshObject,
    const MObject &meshParentObject,
    const MPointArray &basePoints)
{
    AppendImportDebugLog("delta: begin");
    const std::vector<const simple_dmx::Element *> deltaStates = FindAttributeElementArray(document, meshElement, "deltaStates");
    if (deltaStates.empty())
    {
        return MS::kSuccess;
    }

    MDagPath baseParentPath;
    MStatus status = MDagPath::getAPathTo(meshParentObject, baseParentPath);
    if (!status)
    {
        return status;
    }

    MDagPath baseMeshPath;
    status = MDagPath::getAPathTo(meshObject, baseMeshPath);
    if (!status)
    {
        return status;
    }

    MStringArray targetTransforms;
    std::vector<MObject> targetMeshObjects;
    std::vector<std::string> targetNames;
    for (const simple_dmx::Element *deltaState : deltaStates)
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
        duplicateCommand += baseParentPath.fullPathName();
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
            return status;
        }

        const MObject duplicateMeshObject = FindPrimaryMeshChild(duplicateTransformObject);
        if (duplicateMeshObject.isNull())
        {
            return maya_dmx::ReportWarning(MString("maya_dmx: delta target duplicate had no mesh shape: ") + duplicateResult[0]);
        }

        MFnMesh targetMeshFn(duplicateMeshObject, &status);
        if (!status)
        {
            return status;
        }

        MPointArray deltaPoints = basePoints;
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
            return status;
        }

        targetTransforms.append(duplicateResult[0]);
        targetMeshObjects.push_back(duplicateMeshObject);
        targetNames.push_back(deltaState->name.empty() ? std::string("delta") : SanitizeNodeName(deltaState->name));
    }

    if (targetTransforms.length() == 0)
    {
        return MS::kSuccess;
    }

    MFnBlendShapeDeformer blendShapeFn;
    const MObject blendShapeObject = blendShapeFn.create(meshObject, MFnBlendShapeDeformer::kLocalOrigin, &status);
    if (!status)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to create blendShape for ") + baseParentPath.fullPathName(), status);
    }
    const std::string blendShapeName = SanitizeNodeName(meshElement->name.empty() ? std::string("dmx_blendShape") : meshElement->name + "_blendShape");
    MFnDependencyNode blendShapeDependency(blendShapeObject, &status);
    if (!status)
    {
        return status;
    }
    blendShapeDependency.setName(blendShapeName.c_str());
    const MString blendShapeNodeName = blendShapeDependency.name();

    for (unsigned int targetIndex = 0; targetIndex < targetTransforms.length(); ++targetIndex)
    {
        status = blendShapeFn.addTarget(meshObject, static_cast<int>(targetIndex), targetMeshObjects[targetIndex], 1.0);
        if (!status)
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to add blendShape target to ") + blendShapeNodeName, status);
        }
    }

    for (unsigned int targetIndex = 0; targetIndex < targetTransforms.length(); ++targetIndex)
    {
        MString aliasCommand("aliasAttr \"");
        aliasCommand += targetNames[targetIndex].c_str();
        aliasCommand += "\" \"";
        aliasCommand += blendShapeNodeName;
        aliasCommand += ".w[";
        aliasCommand += static_cast<int>(targetIndex);
        aliasCommand += "]\"";
        MGlobal::executeCommand(aliasCommand, false, false);

        MString deleteCommand("delete \"");
        deleteCommand += targetTransforms[targetIndex];
        deleteCommand += "\"";
        MGlobal::executeCommand(deleteCommand, false, false);
    }

    return MS::kSuccess;
}

MStatus CreateMeshShape(const ImportContext &context, const simple_dmx::Element *dagElement, MObject parent)
{
    const simple_dmx::Document &document = context.document;
    const simple_dmx::Element *meshElement = FindAttributeElement(document, dagElement, "shape");
    if (!meshElement || meshElement->type != "DmeMesh")
    {
        return MS::kSuccess;
    }

    const simple_dmx::Element *vertexData = FindMeshVertexData(document, meshElement);
    if (!vertexData)
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh has no bind/base state: ") + dagElement->name.c_str());
    }

    const std::vector<std::string> positionStrings = FindAttributeStringArray(vertexData, "positions");
    const std::vector<std::string> positionIndexStrings = FindAttributeStringArray(vertexData, "positionsIndices");
    if (positionStrings.empty() || positionIndexStrings.empty())
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh is missing positions or positionsIndices: ") + dagElement->name.c_str());
    }

    MPointArray points;
    for (const std::string &positionString : positionStrings)
    {
        const std::vector<double> values = ParseNumberList(positionString);
        if (values.size() < 3)
        {
            continue;
        }

        points.append(values[0], values[1], values[2]);
    }

    std::vector<int> positionIndices;
    positionIndices.reserve(positionIndexStrings.size());
    for (const std::string &indexString : positionIndexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (values.empty())
        {
            continue;
        }

        positionIndices.push_back(static_cast<int>(values[0]));
    }

    const std::vector<std::string> normalStrings = FindAttributeStringArray(vertexData, "normals");
    const std::vector<std::string> normalIndexStrings = FindAttributeStringArray(vertexData, "normalsIndices");
    std::vector<int> normalIndices;
    normalIndices.reserve(normalIndexStrings.size());
    for (const std::string &indexString : normalIndexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (!values.empty())
        {
            normalIndices.push_back(static_cast<int>(values[0]));
        }
    }

    const std::vector<std::string> uvStrings = FindAttributeStringArray(vertexData, "textureCoordinates");
    const std::vector<std::string> uvIndexStrings = FindAttributeStringArray(vertexData, "textureCoordinatesIndices");
    std::vector<int> uvIndices;
    uvIndices.reserve(uvIndexStrings.size());
    for (const std::string &indexString : uvIndexStrings)
    {
        const std::vector<double> values = ParseNumberList(indexString);
        if (!values.empty())
        {
            uvIndices.push_back(static_cast<int>(values[0]));
        }
    }

    const bool flipV = FindAttributeString(vertexData, "flipVCoordinates") == "1" ||
        FindAttributeString(vertexData, "flipVCoordinates") == "true";

    MIntArray polygonCounts;
    MIntArray polygonConnects;
    MIntArray faceIds;
    MIntArray normalVertexIds;
    MVectorArray faceVertexNormals;
    MIntArray uvIds;
    std::vector<FaceSetAssignment> faceSetAssignments;
    for (const simple_dmx::Element *faceSet : FindAttributeElementArray(document, meshElement, "faceSets"))
    {
        const int polygonStart = polygonCounts.length();
        const std::vector<std::string> faceStrings = FindAttributeStringArray(faceSet, "faces");
        std::vector<int> faceIndices;
        faceIndices.reserve(faceStrings.size());
        for (const std::string &faceString : faceStrings)
        {
            const std::vector<double> values = ParseNumberList(faceString);
            if (values.empty())
            {
                continue;
            }

            faceIndices.push_back(static_cast<int>(values[0]));
        }

        int polygonVertexCount = 0;
        int polygonIndex = polygonCounts.length();
        unsigned int polygonConnectStart = polygonConnects.length();
        for (int faceVertexIndex : faceIndices)
        {
            if (faceVertexIndex == -1)
            {
                if (polygonVertexCount >= 3)
                {
                    polygonCounts.append(polygonVertexCount);
                    ++polygonIndex;
                }
                else if (polygonVertexCount > 0)
                {
                    for (unsigned int i = 0; i < static_cast<unsigned int>(polygonVertexCount); ++i)
                    {
                        polygonConnects.remove(polygonConnects.length() - 1);
                        if (faceVertexNormals.length() > 0)
                        {
                            faceVertexNormals.remove(faceVertexNormals.length() - 1);
                            faceIds.remove(faceIds.length() - 1);
                            normalVertexIds.remove(normalVertexIds.length() - 1);
                        }
                        if (uvIds.length() > 0)
                        {
                            uvIds.remove(uvIds.length() - 1);
                        }
                    }
                }

                polygonVertexCount = 0;
                polygonConnectStart = polygonConnects.length();
                continue;
            }

            if (faceVertexIndex < 0 || faceVertexIndex >= static_cast<int>(positionIndices.size()))
            {
                continue;
            }

            const int pointIndex = positionIndices[faceVertexIndex];
            if (pointIndex < 0 || pointIndex >= static_cast<int>(points.length()))
            {
                continue;
            }

            polygonConnects.append(pointIndex);
            if (!normalStrings.empty() && faceVertexIndex < static_cast<int>(normalIndices.size()))
            {
                const int normalIndex = normalIndices[faceVertexIndex];
                if (normalIndex >= 0 && normalIndex < static_cast<int>(normalStrings.size()))
                {
                    const std::vector<double> normalValues = ParseNumberList(normalStrings[normalIndex]);
                    if (normalValues.size() >= 3)
                    {
                        faceVertexNormals.append(MVector(
                            normalValues[0],
                            normalValues[1],
                            normalValues[2]));
                        faceIds.append(polygonIndex);
                        normalVertexIds.append(pointIndex);
                    }
                }
            }

            if (!uvStrings.empty() && faceVertexIndex < static_cast<int>(uvIndices.size()))
            {
                const int uvIndex = uvIndices[faceVertexIndex];
                if (uvIndex >= 0 && uvIndex < static_cast<int>(uvStrings.size()))
                {
                    uvIds.append(uvIndex);
                }
            }

            ++polygonVertexCount;
        }

        if (polygonVertexCount >= 3)
        {
            polygonCounts.append(polygonVertexCount);
        }
        else if (polygonVertexCount > 0)
        {
            while (polygonConnects.length() > polygonConnectStart)
            {
                polygonConnects.remove(polygonConnects.length() - 1);
            }
        }

        const int polygonEnd = polygonCounts.length();
        if (polygonEnd > polygonStart)
        {
            FaceSetAssignment assignment;
            assignment.shadingGroupName = faceSet ? faceSet->name : std::string();
            assignment.polygonStart = polygonStart;
            assignment.polygonCount = polygonEnd - polygonStart;
            faceSetAssignments.push_back(std::move(assignment));
        }
    }

    if (points.length() == 0 || polygonCounts.length() == 0 || polygonConnects.length() == 0)
    {
        return maya_dmx::ReportWarning(MString("maya_dmx: mesh geometry was empty after parsing: ") + dagElement->name.c_str());
    }

    MStatus status;
    MFnMesh meshFn;
    const MObject meshObject = meshFn.create(points.length(), polygonCounts.length(), points, polygonCounts, polygonConnects, parent, &status);
    if (!status)
    {
        return status;
    }

    meshFn.setName((dagElement->name.empty() ? std::string("dmx_meshShape") : dagElement->name + "Shape").c_str());

    if (!uvStrings.empty() && uvIds.length() == polygonConnects.length())
    {
        MFloatArray uValues;
        MFloatArray vValues;
        for (const std::string &uvString : uvStrings)
        {
            const std::vector<double> values = ParseNumberList(uvString);
            if (values.size() < 2)
            {
                uValues.append(0.0f);
                vValues.append(0.0f);
                continue;
            }

            uValues.append(static_cast<float>(values[0]));
            float v = static_cast<float>(values[1]);
            if (flipV)
            {
                v = 1.0f - v;
            }
            vValues.append(v);
        }

        status = meshFn.setUVs(uValues, vValues);
        if (status)
        {
            status = meshFn.assignUVs(polygonCounts, uvIds);
        }
        if (!status)
        {
            return status;
        }
    }

    if (faceVertexNormals.length() == polygonConnects.length() && faceIds.length() == polygonConnects.length() && normalVertexIds.length() == polygonConnects.length())
    {
        status = meshFn.setFaceVertexNormals(faceVertexNormals, faceIds, normalVertexIds, MSpace::kObject);
        if (!status)
        {
            return status;
        }
    }

    status = AssignFaceSetMaterials(meshFn, faceSetAssignments);
    if (!status)
    {
        return status;
    }

    status = ApplyDeltaStates(document, meshElement, meshObject, parent, points);
    if (!status)
    {
        return status;
    }

    return ApplySkinning(context, vertexData, meshObject, parent);
}

MStatus ImportDagRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement,
    MObject parent)
{
    if (!dagElement)
    {
        return MS::kSuccess;
    }

    const bool isJoint = dagElement->type == "DmeJoint" || context.jointSet.find(dagElement) != context.jointSet.end();
    const std::string nodeName = dagElement->name.empty() ? dagElement->type : dagElement->name;

    MStatus status;
    const MObject nodeObject = CreateDagNode(nodeName, isJoint, parent, status);
    if (!status)
    {
        return status;
    }

    MDagPath nodePath;
    status = MDagPath::getAPathTo(nodeObject, nodePath);
    if (!status)
    {
        return status;
    }
    context.importedDagPaths[dagElement] = nodePath;

    status = ApplyTransform(context.document, dagElement, nodeObject);
    if (!status)
    {
        return status;
    }

    status = CreateMeshShape(context, dagElement, nodeObject);
    if (!status)
    {
        return status;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        status = ImportDagRecursive(context, child, nodeObject);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

const simple_dmx::Element *FindImportRoot(const simple_dmx::Document &document)
{
    const simple_dmx::Element *root = document.GetRoot();
    if (!root)
    {
        return nullptr;
    }

    if (root->type == "DmeModel" || root->type == "DmeDag" || root->type == "DmeJoint")
    {
        return root;
    }

    if (const simple_dmx::Element *model = FindAttributeElement(document, root, "model"))
    {
        return model;
    }

    if (const simple_dmx::Element *skeleton = FindAttributeElement(document, root, "skeleton"))
    {
        return skeleton;
    }

    return root;
}
}

void *DmxImportTranslator::Create()
{
    return new DmxImportTranslator();
}

bool DmxImportTranslator::haveReadMethod() const
{
    return true;
}

bool DmxImportTranslator::haveWriteMethod() const
{
    return false;
}

bool DmxImportTranslator::canBeOpened() const
{
    return true;
}

MString DmxImportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxImportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxImportTranslator::reader(const MFileObject &fileObject, const MString &, FileAccessMode)
{
    const std::string fileText = ReadTextFile(fileObject);
    if (fileText.empty())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to read file ") + fileObject.rawFullName());
    }

    simple_dmx::Document document;
    std::string parseError;
    if (!simple_dmx::ParseDocument(fileText, document, parseError))
    {
        return maya_dmx::ReportError(MString("maya_dmx: parse error: ") + parseError.c_str());
    }

    const simple_dmx::Element *importRoot = FindImportRoot(document);
    if (!importRoot)
    {
        return maya_dmx::ReportError("maya_dmx: no importable DMX root element found.");
    }

    ImportContext context{document};
    context.modelRoot = importRoot->type == "DmeModel" ? importRoot : nullptr;
    if (context.modelRoot)
    {
        CollectJointInfo(document, context.modelRoot, context);
    }

    MStatus status;
    MFnTransform rootTransformFn;
    MObject sceneRoot = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }

    rootTransformFn.setName(importRoot->name.empty() ? "dmx_import" : importRoot->name.c_str());

    const std::string upAxis = FindAttributeString(importRoot, "upAxis");
    if (upAxis == "Z")
    {
        rootTransformFn.setRotation(MEulerRotation(-1.57079632679, 0.0, 0.0));
        maya_dmx::ReportWarning("maya_dmx: imported Z-up model with a -90deg X correction group.");
    }

    status = ApplyTransform(document, importRoot, sceneRoot);
    if (!status)
    {
        return status;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document, importRoot, "children"))
    {
        status = ImportDagRecursive(context, child, sceneRoot);
        if (!status)
        {
            return status;
        }
    }

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + fileObject.rawFullName());
}
