#include "DmxImportTranslator.h"

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
#include <maya/MEulerRotation.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
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
    std::string materialName;
    std::string shaderName;
    std::string shaderType;
    std::string color;
    std::string transparency;
    std::string diffuseTexture;
    std::string normalTexture;
    std::string bumpTexture;
    int polygonStart = 0;
    int polygonCount = 0;
};

struct UvSetData
{
    int channelIndex = 0;
    std::string attributeName;
    std::string indexAttributeName;
    std::string mayaSetName;
    std::vector<std::string> values;
    std::vector<int> indices;
    MIntArray polygonVertexIndices;
};

struct DeltaStateGroup
{
    std::string nodeName;
    std::vector<const simple_dmx::Element *> states;
};

struct ImportContext
{
    const simple_dmx::Document &document;
    const simple_dmx::Element *modelRoot = nullptr;
    std::vector<std::string> jointOrder;
    std::unordered_map<std::string, MDagPath> importedDagPaths;
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
};

struct ImportOptions
{
    bool importSkin = true;
    bool importMaterials = true;
    bool importDeltaStates = true;
};

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

int ParseUvChannelIndex(const std::string &attributeName)
{
    if (attributeName == "textureCoordinates")
    {
        return 0;
    }

    if (attributeName.rfind("texcoord$", 0) == 0)
    {
        const char *suffix = attributeName.c_str() + 9;
        if (*suffix == '\0')
        {
            return -1;
        }

        char *end = nullptr;
        const long parsedIndex = std::strtol(suffix, &end, 10);
        if (end && *end == '\0' && parsedIndex >= 1)
        {
            return static_cast<int>(parsedIndex);
        }
    }

    return -1;
}

std::vector<UvSetData> CollectUvSets(const simple_dmx::Element *vertexData)
{
    std::vector<UvSetData> uvSets;
    if (!vertexData)
    {
        return uvSets;
    }

    const std::vector<std::string> mayaUvSetNames = FindAttributeStringArray(vertexData, "mayaUvSetNames");
    for (const auto &entry : vertexData->attributes)
    {
        const int channelIndex = ParseUvChannelIndex(entry.first);
        if (channelIndex < 0 || entry.second.kind != simple_dmx::Attribute::Kind::StringArray)
        {
            continue;
        }

        UvSetData uvSet;
        uvSet.channelIndex = channelIndex;
        uvSet.attributeName = entry.first;
        uvSet.indexAttributeName = entry.first + "Indices";
        uvSet.values = entry.second.stringArray;
        uvSet.indices.reserve(FindAttributeStringArray(vertexData, uvSet.indexAttributeName.c_str()).size());
        for (const std::string &indexString : FindAttributeStringArray(vertexData, uvSet.indexAttributeName.c_str()))
        {
            const std::vector<double> values = ParseNumberList(indexString);
            if (!values.empty())
            {
                uvSet.indices.push_back(static_cast<int>(values[0]));
            }
        }

        if (channelIndex >= 0 && channelIndex < static_cast<int>(mayaUvSetNames.size()))
        {
            uvSet.mayaSetName = mayaUvSetNames[static_cast<size_t>(channelIndex)];
        }
        else
        {
            uvSet.mayaSetName = channelIndex == 0 ? "map1" : entry.first;
        }

        uvSets.push_back(std::move(uvSet));
    }

    std::sort(
        uvSets.begin(),
        uvSets.end(),
        [](const UvSetData &lhs, const UvSetData &rhs)
        {
            return lhs.channelIndex < rhs.channelIndex;
        });
    return uvSets;
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
MStatus CreateSkinClusterWithApi(const simple_dmx::Element *vertexData, const MDagPathArray &influencePaths, const MDagPath &meshDagPath, const MDagPath &meshParentPath, MObject &skinClusterObject);
MStatus RestoreSkinClusterSettings(const simple_dmx::Element *vertexData, const MObject &skinClusterObject);

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

    std::vector<bool> referencedJointMask(context.jointOrder.size(), false);
    size_t skippedJointReferenceCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < static_cast<unsigned int>(vertexCount); ++vertexIndex)
    {
        const size_t baseOffset = static_cast<size_t>(vertexIndex) * static_cast<size_t>(jointCount);
        for (int slot = 0; slot < jointCount; ++slot)
        {
            const int dmxJointIndex = jointIndices[baseOffset + slot];
            if (dmxJointIndex < 0 || static_cast<size_t>(dmxJointIndex) >= context.jointOrder.size())
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

    MStatus status;
    MDagPathArray activeInfluencePaths;
    std::vector<int> activeDmxJointIndices;
    for (size_t dmxJointIndex = 0; dmxJointIndex < context.jointOrder.size(); ++dmxJointIndex)
    {
        if (!referencedJointMask[dmxJointIndex])
        {
            continue;
        }

        auto it = context.importedDagPaths.find(context.jointOrder[dmxJointIndex]);
        if (it == context.importedDagPaths.end())
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
    status = CreateSkinClusterWithApi(vertexData, activeInfluencePaths, meshDagPath, meshParentPath, skinClusterObject);
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
    for (unsigned int influencePathIndex = 0; influencePathIndex < activeInfluencePaths.length(); ++influencePathIndex)
    {
        const unsigned int influenceIndex = skinClusterFn.indexForInfluenceObject(activeInfluencePaths[influencePathIndex], &status);
        if (!status)
        {
            return status;
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
        return status;
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
        const unsigned int temporaryMaxInfluences = std::max(1u, maxAssignedInfluences);
        maxInfluencesPlug.setInt(static_cast<int>(temporaryMaxInfluences));
    }

    status = skinClusterFn.setWeights(meshDagPath, vertexComponent, influenceIndices, weights, false);
    if (status)
    {
        AppendImportDebugLog("skinning: setWeights ok");
    }
    if (!status)
    {
        return status;
    }

    return RestoreSkinClusterSettings(vertexData, skinClusterObject);
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

    MStatus status;
    MDagPath meshPath;
    status = meshFn.getPath(meshPath);
    if (!status)
    {
        return status;
    }

    for (const FaceSetAssignment &assignment : faceSetAssignments)
    {
        if (assignment.polygonCount <= 0 || assignment.shadingGroupName.empty())
        {
            continue;
        }

        std::string shadingGroupName = SanitizeNodeName(assignment.shadingGroupName);
        if (shadingGroupName.empty())
        {
            shadingGroupName = "dmxMaterialSet";
        }
        if (shadingGroupName.size() < 3 || shadingGroupName.substr(shadingGroupName.size() - 3) != "_SG")
        {
            shadingGroupName += "_SG";
        }
        const std::string materialName = assignment.materialName.empty() ? shadingGroupName : assignment.materialName;
        const std::string shaderType = assignment.shaderType.empty() ? "lambert" : assignment.shaderType;
        const std::string shaderName = assignment.shaderName.empty() ?
            SanitizeNodeName(materialName) :
            SanitizeNodeName(assignment.shaderName);

        MObject shadingGroupObject = EnsureShadingGroup(shadingGroupName, status);
        if (!status || shadingGroupObject.isNull())
        {
            return status;
        }

        MObject shaderObject = EnsureDependencyNode(shaderType, shaderName, status);
        if (!status || shaderObject.isNull())
        {
            return status;
        }

        MFnDependencyNode shaderNodeFn(shaderObject, &status);
        if (!status)
        {
            return status;
        }

        MFnDependencyNode shadingGroupNodeFn(shadingGroupObject, &status);
        if (!status)
        {
            return status;
        }

        MPlug surfaceShaderPlug = shadingGroupNodeFn.findPlug("surfaceShader", true, &status);
        if (!status)
        {
            return status;
        }

        MPlug outColorPlug = shaderNodeFn.findPlug("outColor", true, &status);
        if (!status)
        {
            return status;
        }

        status = ConnectPlugs(outColorPlug, surfaceShaderPlug);
        if (!status)
        {
            return status;
        }

        MPlug colorPlug = shaderNodeFn.findPlug("color", true, &status);
        if (status && !assignment.color.empty())
        {
            SetVector3Plug(colorPlug, assignment.color);
        }

        MPlug transparencyPlug = shaderNodeFn.findPlug("transparency", true, &status);
        if (status && !assignment.transparency.empty())
        {
            SetVector3Plug(transparencyPlug, assignment.transparency);
        }

        if (!assignment.diffuseTexture.empty() && colorPlug.isNull() == false)
        {
            status = AssignTextureToShader(shaderName + "_diffuseFile", assignment.diffuseTexture, colorPlug, false);
            if (!status)
            {
                return status;
            }
        }

        MPlug normalCameraPlug = shaderNodeFn.findPlug("normalCamera", true, &status);
        const std::string normalOrBumpTexture = assignment.normalTexture.empty() ? assignment.bumpTexture : assignment.normalTexture;
        if (status && !normalOrBumpTexture.empty())
        {
            MObject bumpNodeObject = EnsureDependencyNode("bump2d", shaderName + "_normalBump", status);
            if (!status || bumpNodeObject.isNull())
            {
                return status;
            }

            MFnDependencyNode bumpNodeFn(bumpNodeObject, &status);
            if (!status)
            {
                return status;
            }

            MPlug bumpInterpPlug = bumpNodeFn.findPlug("bumpInterp", true, &status);
            if (status)
            {
                bumpInterpPlug.setInt(1);
            }

            MPlug bumpValuePlug = bumpNodeFn.findPlug("bumpValue", true, &status);
            if (!status)
            {
                return status;
            }

            status = AssignTextureToShader(shaderName + "_normalFile", normalOrBumpTexture, bumpValuePlug, true);
            if (!status)
            {
                return status;
            }

            MPlug outNormalPlug = bumpNodeFn.findPlug("outNormal", true, &status);
            if (!status)
            {
                return status;
            }

            status = ConnectPlugs(outNormalPlug, normalCameraPlug);
            if (!status)
            {
                return status;
            }
        }

        MFnSingleIndexedComponent componentFn;
        MObject faceComponent = componentFn.create(MFn::kMeshPolygonComponent, &status);
        if (!status)
        {
            return status;
        }

        MIntArray faceIds;
        for (int offset = 0; offset < assignment.polygonCount; ++offset)
        {
            faceIds.append(assignment.polygonStart + offset);
        }

        status = componentFn.addElements(faceIds);
        if (!status)
        {
            return status;
        }

        MFnSet shadingGroupSetFn(shadingGroupObject, &status);
        if (!status)
        {
            return status;
        }

        status = shadingGroupSetFn.addMember(meshPath, faceComponent);
        if (!status)
        {
            return status;
        }
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

MStatus CreateSkinClusterWithApi(
    const simple_dmx::Element *vertexData,
    const MDagPathArray &influencePaths,
    const MDagPath &meshDagPath,
    const MDagPath &meshParentPath,
    MObject &skinClusterObject)
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

    const std::string requestedSkinClusterName = FindAttributeString(vertexData, "mayaSkinClusterName");
    if (!requestedSkinClusterName.empty())
    {
        skinClusterNodeFn.setName(requestedSkinClusterName.c_str(), &status);
        status = MS::kSuccess;
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

    const std::vector<std::string> bindPreMatrixStrings = FindAttributeStringArray(vertexData, "mayaBindPreMatrix");
    const std::vector<std::string> influencePathStrings = FindAttributeStringArray(vertexData, "mayaInfluencePaths");
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
        MMatrix bindPreMatrix = influencePaths[influenceIndex].inclusiveMatrixInverse();
        if (influenceIndex < bindPreMatrixStrings.size())
        {
            MMatrix parsedMatrix;
            if (ParseMatrixString(bindPreMatrixStrings[influenceIndex], parsedMatrix))
            {
                bindPreMatrix = parsedMatrix;
            }
        }
        else if (influenceIndex < influencePathStrings.size() &&
            influencePathStrings[influenceIndex] != influencePaths[influenceIndex].fullPathName().asChar())
        {
            AppendImportDebugLog("skinning: influence path order mismatch while restoring bindPreMatrix");
        }

        MObject bindPreMatrixObject = matrixDataFn.create(bindPreMatrix, &status);
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
    MMatrix geomMatrix = meshParentPath.inclusiveMatrix();
    MMatrix parsedGeomMatrix;
    if (ParseMatrixString(FindAttributeString(vertexData, "mayaGeomMatrix"), parsedGeomMatrix))
    {
        geomMatrix = parsedGeomMatrix;
    }
    MObject geomMatrixObject = geomMatrixDataFn.create(geomMatrix, &status);
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

MStatus RestoreSkinClusterSettings(const simple_dmx::Element *vertexData, const MObject &skinClusterObject)
{
    MStatus status;
    MFnDependencyNode skinClusterNode(skinClusterObject, &status);
    if (!status)
    {
        return status;
    }

    MPlug skinningMethodPlug = skinClusterNode.findPlug("skinningMethod", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData, "mayaSkinningMethod"));
        if (!values.empty())
        {
            skinningMethodPlug.setShort(static_cast<short>(values[0]));
        }
    }
    status = MS::kSuccess;

    MPlug useComponentsPlug = skinClusterNode.findPlug("useComponents", true, &status);
    if (status)
    {
        const std::string value = FindAttributeString(vertexData, "mayaUseComponents");
        if (!value.empty())
        {
            useComponentsPlug.setBool(value == "1" || value == "true");
        }
    }
    status = MS::kSuccess;

    MPlug maxInfluencesPlug = skinClusterNode.findPlug("maxInfluences", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData, "mayaMaxInfluences"));
        if (!values.empty())
        {
            maxInfluencesPlug.setInt(static_cast<int>(values[0]));
        }
    }
    status = MS::kSuccess;

    MPlug maintainMaxInfluencesPlug = skinClusterNode.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        const std::string value = FindAttributeString(vertexData, "mayaMaintainMaxInfluences");
        if (!value.empty())
        {
            maintainMaxInfluencesPlug.setBool(value == "1" || value == "true");
        }
    }
    status = MS::kSuccess;

    MPlug normalizeWeightsPlug = skinClusterNode.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        const std::vector<double> values = ParseNumberList(FindAttributeString(vertexData, "mayaNormalizeWeights"));
        if (!values.empty())
        {
            normalizeWeightsPlug.setShort(static_cast<short>(values[0]));
        }
    }

    return MS::kSuccess;
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
            groupName = meshElement->name.empty() ? std::string("dmx_blendShape") : meshElement->name + "_blendShape";
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
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn;
        const MObject blendShapeObject = blendShapeFn.create(meshObject, MFnBlendShapeDeformer::kLocalOrigin, &status);
        if (!status)
        {
            return maya_dmx::ReportError(MString("maya_dmx: failed to create blendShape for ") + baseParentPath.fullPathName(), status);
        }
        const std::string blendShapeName = SanitizeNodeName(group.nodeName);
        MFnDependencyNode blendShapeDependency(blendShapeObject, &status);
        if (!status)
        {
            return status;
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
        status = ApplyDeltaStates(document, meshElement, meshObject, parent, points);
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
    const ImportContext &context,
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

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + fileObject.rawFullName());
}
