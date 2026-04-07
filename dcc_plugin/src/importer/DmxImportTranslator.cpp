#include "DmxImportTranslator.h"
#include "DmxImportTranslatorTypes.h"
#include "DmxImportUtils.h"

#include "../common/MayaDmxCommon.h"
#include "../common/SimpleDmxText.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <Windows.h>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDagModifier.h>
#include <maya/MDGModifier.h>
#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnSet.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFloatArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MPlugArray.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVectorArray.h>
#include <maya/MVector.h>

namespace
{
using dmx_import_utils::FindAttributeElement;
using dmx_import_utils::FindAttributeElementArray;
using dmx_import_utils::FindAttributeString;
using dmx_import_utils::FindAttributeStringArray;
using dmx_import_utils::ParseNumberList;
using dmx_import_utils::SanitizeNodeName;

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

struct DeltaStateGroup
{
    std::string nodeName;
    std::vector<const simple_dmx::Element *> states;
};

using dmx_import_translator::BlendShapeTargetBinding;
using dmx_import_translator::ImportContext;
using dmx_import_translator::ImportOptions;
using dmx_import_translator::ScalarAttributeBinding;

std::unordered_map<std::string, std::string> ParseOptionMap(const MString &options)
{
    std::unordered_map<std::string, std::string> optionMap;
    std::string text = options.asChar();
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find(';', start);
        if (end == std::string::npos)
        {
            end = text.size();
        }

        const std::string pair = text.substr(start, end - start);
        const size_t separator = pair.find('=');
        if (separator != std::string::npos)
        {
            std::string key = pair.substr(0, separator);
            std::string value = pair.substr(separator + 1);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            optionMap[key] = value;
        }

        start = end + 1;
    }
    return optionMap;
}

bool ParseBoolOption(const std::unordered_map<std::string, std::string> &optionMap, const char *key, bool defaultValue)
{
    auto it = optionMap.find(key);
    if (it == optionMap.end())
    {
        return defaultValue;
    }

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "1" || value == "true" || value == "yes";
}

ImportOptions ParseImportOptions(const MString &options)
{
    ImportOptions importOptions;
    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    importOptions.importSkin = ParseBoolOption(optionMap, "importskin", true);
    importOptions.importMaterials = ParseBoolOption(optionMap, "importmaterials", true);
    importOptions.importDeltaStates = ParseBoolOption(optionMap, "importdeltastates", true);
    return importOptions;
}

std::string ReadTextFile(const MFileObject &fileObject)
{
    std::ifstream file(fileObject.rawFullName().asChar(), std::ios::in | std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

MStatus SetOrCreateStringAttribute(const MObject &nodeObject, const char *attributeName, const std::string &value)
{
    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return status;
    }

    MObject attributeObject = nodeFn.attribute(attributeName, &status);
    if (!status || attributeObject.isNull())
    {
        MFnTypedAttribute typedAttributeFn;
        attributeObject = typedAttributeFn.create(attributeName, attributeName, MFnData::kString, MObject::kNullObj, &status);
        if (!status)
        {
            return status;
        }
        typedAttributeFn.setHidden(true);
        typedAttributeFn.setStorable(true);
        typedAttributeFn.setReadable(true);
        typedAttributeFn.setWritable(true);
        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return status;
        }
    }

    MPlug attributePlug = nodeFn.findPlug(attributeName, true, &status);
    if (!status)
    {
        return status;
    }

    return attributePlug.setString(value.c_str());
}

std::string JoinLines(const std::vector<std::string> &values)
{
    std::ostringstream stream;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            stream << '\n';
        }
        stream << values[i];
    }
    return stream.str();
}

bool ParseFloat3(const std::string &text, float (&components)[3])
{
    const std::vector<double> values = ParseNumberList(text);
    if (values.size() < 3)
    {
        return false;
    }

    components[0] = static_cast<float>(values[0]);
    components[1] = static_cast<float>(values[1]);
    components[2] = static_cast<float>(values[2]);
    return true;
}

std::string NormalizeAxisName(std::string axisName)
{
    std::transform(axisName.begin(), axisName.end(), axisName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return axisName;
}

bool ComputeRootAxisCorrection(const std::string &sourceUpAxis, MEulerRotation &outRotation, MString &outWarning)
{
    const std::string normalizedSourceUpAxis = NormalizeAxisName(sourceUpAxis);
    if (normalizedSourceUpAxis.empty())
    {
        return false;
    }

    MStatus status;
    const bool mayaYAxisUp = MGlobal::isYAxisUp(&status);
    if (!status)
    {
        return false;
    }

    const bool mayaZAxisUp = MGlobal::isZAxisUp(&status);
    if (!status)
    {
        return false;
    }

    if (normalizedSourceUpAxis == "Z" && mayaYAxisUp)
    {
        outRotation = MEulerRotation(-1.57079632679, 0.0, 0.0);
        outWarning = "maya_dmx: imported Z-up model into a Y-up Maya scene with a -90deg X correction group.";
        return true;
    }

    if (normalizedSourceUpAxis == "Y" && mayaZAxisUp)
    {
        outRotation = MEulerRotation(1.57079632679, 0.0, 0.0);
        outWarning = "maya_dmx: imported Y-up model into a Z-up Maya scene with a +90deg X correction group.";
        return true;
    }

    return false;
}

std::string ElementKey(const simple_dmx::Element *element)
{
    if (!element)
    {
        return {};
    }

    if (!element->id.empty())
    {
        return element->id;
    }

    std::ostringstream stream;
    stream << reinterpret_cast<uintptr_t>(element);
    return stream.str();
}

bool ParseMatrixString(const std::string &text, MMatrix &matrix)
{
    const std::vector<double> values = ParseNumberList(text);
    if (values.size() < 16)
    {
        return false;
    }

    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            matrix[row][column] = values[row * 4 + column];
        }
    }

    return true;
}

#include "DmxImportAnimation.hpp"
#include "DmxImportDeformers.hpp"

MObject FindNodeByName(const std::string &nodeName, MStatus *outStatus = nullptr)
{
    MStatus status;
    MSelectionList selectionList;
    status = selectionList.add(nodeName.c_str());
    if (!status)
    {
        if (outStatus)
        {
            *outStatus = status;
        }
        return MObject::kNullObj;
    }

    MObject nodeObject;
    status = selectionList.getDependNode(0, nodeObject);
    if (outStatus)
    {
        *outStatus = status;
    }
    return status ? nodeObject : MObject::kNullObj;
}

MStatus SetVector3Plug(const MPlug &plug, const std::string &value)
{
    float components[3] = {};
    if (!ParseFloat3(value, components) || plug.numChildren() < 3)
    {
        return MS::kFailure;
    }

    MStatus status = plug.child(0).setFloat(components[0]);
    if (!status)
    {
        return status;
    }
    status = plug.child(1).setFloat(components[1]);
    if (!status)
    {
        return status;
    }
    return plug.child(2).setFloat(components[2]);
}

MStatus DisconnectDestinationPlug(MDGModifier &modifier, const MPlug &destinationPlug)
{
    MPlugArray sourcePlugs;
    destinationPlug.connectedTo(sourcePlugs, true, false);
    MStatus status = MS::kSuccess;
    if (destinationPlug.isNull())
    {
        return status;
    }

    for (unsigned int sourceIndex = 0; sourceIndex < sourcePlugs.length(); ++sourceIndex)
    {
        status = modifier.disconnect(sourcePlugs[sourceIndex], destinationPlug);
        if (!status)
        {
            return status;
        }
    }

    return modifier.doIt();
}

MStatus ConnectPlugs(MPlug sourcePlug, MPlug destinationPlug)
{
    MDGModifier modifier;
    MStatus status = DisconnectDestinationPlug(modifier, destinationPlug);
    if (!status)
    {
        return status;
    }

    status = modifier.connect(sourcePlug, destinationPlug);
    if (!status)
    {
        return status;
    }

    return modifier.doIt();
}

MObject EnsureDependencyNode(const std::string &nodeType, const std::string &requestedName, MStatus &status)
{
    MStatus lookupStatus;
    MObject existingNode = FindNodeByName(requestedName, &lookupStatus);
    if (lookupStatus && !existingNode.isNull())
    {
        MFnDependencyNode existingNodeFn(existingNode, &status);
        if (!status)
        {
            return MObject::kNullObj;
        }

        if (existingNodeFn.typeName() == nodeType.c_str())
        {
            return existingNode;
        }
    }

    MDGModifier modifier;
    MObject nodeObject = modifier.createNode(nodeType.c_str(), &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    status = modifier.doIt();
    if (!status)
    {
        return MObject::kNullObj;
    }

    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    nodeFn.setName(requestedName.c_str(), &status);
    if (!status)
    {
        return MObject::kNullObj;
    }
    return nodeObject;
}

MObject EnsureShadingGroup(const std::string &requestedName, MStatus &status)
{
    MStatus lookupStatus;
    MObject existingNode = FindNodeByName(requestedName, &lookupStatus);
    if (lookupStatus && !existingNode.isNull() && existingNode.hasFn(MFn::kSet))
    {
        MFnSet existingSet(existingNode, &status);
        if (status && existingSet.restriction() == MFnSet::kRenderableOnly)
        {
            return existingNode;
        }
    }

    MSelectionList emptyList;
    MFnSet setFn;
    MObject setObject = setFn.create(emptyList, MFnSet::kRenderableOnly, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    setFn.setName(requestedName.c_str(), &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    return setObject;
}

MStatus AssignTextureToShader(
    const std::string &fileNodeName,
    const std::string &texturePath,
    MPlug destinationPlug,
    bool useAlphaOutput)
{
    if (texturePath.empty())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MObject fileNodeObject = EnsureDependencyNode("file", fileNodeName, status);
    if (!status)
    {
        return status;
    }

    MFnDependencyNode fileNodeFn(fileNodeObject, &status);
    if (!status)
    {
        return status;
    }

    MPlug fileTextureNamePlug = fileNodeFn.findPlug("fileTextureName", true, &status);
    if (!status)
    {
        return status;
    }
    status = fileTextureNamePlug.setString(texturePath.c_str());
    if (!status)
    {
        return status;
    }

    MPlug outputPlug = fileNodeFn.findPlug(useAlphaOutput ? "outAlpha" : "outColor", true, &status);
    if (!status)
    {
        return status;
    }

    return ConnectPlugs(outputPlug, destinationPlug);
}

#include "DmxImportMeshMaterial.hpp"

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
            context.jointOrder.push_back(ElementKey(joint));
        }
    }
}


MStatus CreateMeshShape(ImportContext &context, const simple_dmx::Element *dagElement, MObject parent)
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

    std::vector<UvSetData> uvSets = CollectUvSets(vertexData);
    const std::vector<std::string> tangentStrings = FindAttributeStringArray(vertexData, "tangents");
    const std::vector<std::string> tangentIndexStrings = FindAttributeStringArray(vertexData, "tangentsIndices");

    const bool flipV = FindAttributeString(vertexData, "flipVCoordinates") == "1" ||
        FindAttributeString(vertexData, "flipVCoordinates") == "true";

    MIntArray polygonCounts;
    MIntArray polygonConnects;
    MIntArray faceIds;
    MIntArray normalVertexIds;
    MVectorArray faceVertexNormals;
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
                        for (UvSetData &uvSet : uvSets)
                        {
                            if (uvSet.polygonVertexIndices.length() > 0)
                            {
                                uvSet.polygonVertexIndices.remove(uvSet.polygonVertexIndices.length() - 1);
                            }
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

            for (UvSetData &uvSet : uvSets)
            {
                if (faceVertexIndex < static_cast<int>(uvSet.indices.size()))
                {
                    const int uvIndex = uvSet.indices[faceVertexIndex];
                    if (uvIndex >= 0 && uvIndex < static_cast<int>(uvSet.values.size()))
                    {
                        uvSet.polygonVertexIndices.append(uvIndex);
                    }
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
            if (const simple_dmx::Element *materialElement = FindAttributeElement(document, faceSet, "material"))
            {
                assignment.materialName = materialElement->name;
                const std::string materialSlotName = FindAttributeString(materialElement, "mtlName");
                if (!materialSlotName.empty())
                {
                    assignment.materialName = materialSlotName;
                }
                assignment.shaderName = FindAttributeString(materialElement, "mayaShaderName");
                assignment.shaderType = FindAttributeString(materialElement, "mayaShaderType");
                assignment.color = FindAttributeString(materialElement, "mayaColor");
                assignment.transparency = FindAttributeString(materialElement, "mayaTransparency");
                assignment.diffuseTexture = FindAttributeString(materialElement, "mayaDiffuseTexture");
                assignment.normalTexture = FindAttributeString(materialElement, "mayaNormalTexture");
                assignment.bumpTexture = FindAttributeString(materialElement, "mayaBumpTexture");
            }
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

    for (size_t uvSetIndex = 0; uvSetIndex < uvSets.size(); ++uvSetIndex)
    {
        MFloatArray uValues;
        MFloatArray vValues;
        for (const std::string &uvString : uvSets[uvSetIndex].values)
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

        if (uvSets[uvSetIndex].polygonVertexIndices.length() != polygonConnects.length())
        {
            continue;
        }

        MString uvSetName = uvSets[uvSetIndex].mayaSetName.c_str();
        if (uvSetName.length() == 0)
        {
            uvSetName = uvSetIndex == 0 ? meshFn.currentUVSetName() : MString(uvSets[uvSetIndex].attributeName.c_str());
        }

        if (uvSetIndex == 0)
        {
            const MString currentUvSetName = meshFn.currentUVSetName();
            if (uvSetName != currentUvSetName)
            {
                meshFn.renameUVSet(currentUvSetName, uvSetName);
            }
        }
        else
        {
            MStringArray existingUvSetNames;
            meshFn.getUVSetNames(existingUvSetNames);
            bool uvSetExists = false;
            for (unsigned int existingIndex = 0; existingIndex < existingUvSetNames.length(); ++existingIndex)
            {
                if (existingUvSetNames[existingIndex] == uvSetName)
                {
                    uvSetExists = true;
                    break;
                }
            }
            if (!uvSetExists)
            {
                meshFn.createUVSetWithName(uvSetName);
            }
        }

        status = meshFn.setUVs(uValues, vValues, &uvSetName);
        if (status)
        {
            status = meshFn.assignUVs(polygonCounts, uvSets[uvSetIndex].polygonVertexIndices, &uvSetName);
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

    if (context.importMaterials)
    {
        status = AssignFaceSetMaterials(meshFn, faceSetAssignments);
        if (!status)
        {
            return status;
        }
    }

    if (!tangentStrings.empty() && !tangentIndexStrings.empty())
    {
        status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangents", JoinLines(tangentStrings));
        if (!status)
        {
            return status;
        }

        status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangentsIndices", JoinLines(tangentIndexStrings));
        if (!status)
        {
            return status;
        }

        const std::string tangentUvSetName = FindAttributeString(vertexData, "mayaTangentUvSetName");
        if (!tangentUvSetName.empty())
        {
            status = SetOrCreateStringAttribute(meshObject, "mayaDmxTangentUvSetName", tangentUvSetName);
            if (!status)
            {
                return status;
            }
        }
    }

    if (context.importSkin)
    {
        status = ApplySkinning(context, vertexData, meshObject, parent);
        if (!status)
        {
            return status;
        }
    }

    if (context.importDeltaStates)
    {
        status = ApplyDeltaStates(context, document, meshElement, meshObject, parent, points);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

MStatus ImportDagHierarchyRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement,
    MObject parent)
{
    if (!dagElement)
    {
        return MS::kSuccess;
    }

    const bool isJoint = dagElement->type == "DmeJoint";
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
    context.importedDagPaths[ElementKey(dagElement)] = nodePath;
    if (const simple_dmx::Element *transformElement = FindAttributeElement(context.document, dagElement, "transform"))
    {
        context.importedTransformPaths[ElementKey(transformElement)] = nodePath;
    }
    if (SanitizeNodeName(dagElement->name).size() >= 9 &&
        SanitizeNodeName(dagElement->name).rfind("_controls") == SanitizeNodeName(dagElement->name).size() - 9)
    {
        context.importedControlPaths.push_back(nodePath);
    }

    status = ApplyTransform(context.document, dagElement, nodeObject);
    if (!status)
    {
        return status;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        status = ImportDagHierarchyRecursive(context, child, nodeObject);
        if (!status)
        {
            return status;
        }
    }

    return MS::kSuccess;
}

MStatus ImportDagShapesRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement)
{
    if (!dagElement)
    {
        return MS::kSuccess;
    }

    const std::string elementKey = ElementKey(dagElement);
    auto it = context.importedDagPaths.find(elementKey);
    if (it == context.importedDagPaths.end())
    {
        return maya_dmx::ReportError(MString("maya_dmx: imported DAG path was missing for ") + dagElement->name.c_str());
    }

    MStatus status;
    MObject nodeObject = it->second.node(&status);
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
        status = ImportDagShapesRecursive(context, child);
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

MStatus DmxImportTranslator::reader(const MFileObject &fileObject, const MString &options, FileAccessMode)
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

    const ImportOptions importOptions = ParseImportOptions(options);

    ImportContext context{document};
    context.modelRoot = importRoot->type == "DmeModel" ? importRoot : nullptr;
    context.importSkin = importOptions.importSkin;
    context.importMaterials = importOptions.importMaterials;
    context.importDeltaStates = importOptions.importDeltaStates;
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
    MEulerRotation rootAxisCorrection;
    MString rootAxisWarning;
    if (ComputeRootAxisCorrection(upAxis, rootAxisCorrection, rootAxisWarning))
    {
        status = rootTransformFn.setRotation(rootAxisCorrection);
        if (!status)
        {
            return status;
        }
        maya_dmx::ReportWarning(rootAxisWarning);
    }

    status = ApplyTransform(document, importRoot, sceneRoot);
    if (!status)
    {
        return status;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document, importRoot, "children"))
    {
        status = ImportDagHierarchyRecursive(context, child, sceneRoot);
        if (!status)
        {
            return status;
        }
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document, importRoot, "children"))
    {
        status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            return status;
        }
    }

    const simple_dmx::Element *combinationOperator = FindCombinationOperator(document, document.GetRoot(), importRoot, context.modelRoot);
    status = CreateCombinationControls(context, combinationOperator, sceneRoot);
    if (!status)
    {
        return status;
    }

    const simple_dmx::Element *animationList = FindAnimationList(document, document.GetRoot(), importRoot, context.modelRoot);
    if (animationList)
    {
        const std::vector<const simple_dmx::Element *> animations = FindAttributeElementArray(document, animationList, "animations");
        for (const simple_dmx::Element *animation : animations)
        {
            status = ApplyChannelsClipAnimation(context, animation);
            if (!status)
            {
                return status;
            }
        }
    }

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + fileObject.rawFullName());
}
