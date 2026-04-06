#include "DmxImportTranslator.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxText.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFloatArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVectorArray.h>
#include <maya/MVector.h>

namespace
{
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

MStatus ApplySkinning(const ImportContext &context, const simple_dmx::Element *vertexData, const MObject &meshObject, const MObject &meshParentObject)
{
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

    MString command("skinCluster -tsb");
    std::vector<const simple_dmx::Element *> activeJoints;
    activeJoints.reserve(context.jointOrder.size());
    for (const simple_dmx::Element *jointElement : context.jointOrder)
    {
        auto it = context.importedDagPaths.find(jointElement);
        if (it == context.importedDagPaths.end())
        {
            continue;
        }

        activeJoints.push_back(jointElement);
        command += " \"";
        command += it->second.fullPathName();
        command += "\"";
    }

    if (activeJoints.empty())
    {
        return MS::kSuccess;
    }

    MDagPath meshParentPath;
    MStatus status = MDagPath::getAPathTo(meshParentObject, meshParentPath);
    if (!status)
    {
        return status;
    }

    command += " \"";
    command += meshParentPath.fullPathName();
    command += "\"";

    MString skinClusterNodeName;
    status = MGlobal::executeCommand(command, skinClusterNodeName);
    if (!status)
    {
        return status;
    }

    MSelectionList selectionList;
    selectionList.add(skinClusterNodeName);

    MObject skinClusterObject;
    status = selectionList.getDependNode(0, skinClusterObject);
    if (!status)
    {
        return status;
    }

    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
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
    for (unsigned int dmxJointIndex = 0; dmxJointIndex < static_cast<unsigned int>(context.jointOrder.size()); ++dmxJointIndex)
    {
        auto it = context.importedDagPaths.find(context.jointOrder[dmxJointIndex]);
        if (it == context.importedDagPaths.end())
        {
            continue;
        }

        const unsigned int influenceIndex = skinClusterFn.indexForInfluenceObject(it->second, &status);
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

    return skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
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
