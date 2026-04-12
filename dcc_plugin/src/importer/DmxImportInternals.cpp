#include "DmxImportInternals.h"

#include <common/ImportPolicy.h>
#include <common/ImportTransformCorrection.h>

#include <cstdint>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <Windows.h>

#include <maya/MEulerRotation.h>
#include <maya/MDGModifier.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MTransformationMatrix.h>

namespace dmx_import_impl
{

// --- Private helpers (not declared in DmxImportInternals.h) ---

static std::string GetImportDebugLogPath()
{
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }

    std::string logPath(tempPath);
    logPath += "maya_dmx_import_debug.log";
    return logPath;
}

static MObject FindNodeByName(const std::string &nodeName, MStatus *outStatus = nullptr)
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

static MStatus DisconnectDestinationPlug(MDGModifier &modifier, const MPlug &destinationPlug)
{
    MPlugArray sourcePlugs;
    destinationPlug.connectedTo(sourcePlugs, true, false);
    MStatus status = MS::kSuccess;
    if (destinationPlug.isNull())
    {
        return MStatus::kFailure;
    }

    for (unsigned int sourceIndex = 0; sourceIndex < sourcePlugs.length(); ++sourceIndex)
    {
        status = modifier.disconnect(sourcePlugs[sourceIndex], destinationPlug);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return modifier.doIt();
}

// --- Debug helper ---

void ResetImportDebugLog()
{
    const std::string logPath = GetImportDebugLogPath();
    if (logPath.empty())
    {
        return;
    }

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::trunc);
}

void AppendImportDebugLog(const char *message)
{
    const std::string logPath = GetImportDebugLogPath();
    if (logPath.empty())
    {
        return;
    }

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        return;
    }

    logFile << message << "\n";
}

// --- Element helpers ---

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

ImportOptions ParseImportOptions(const MString &options)
{
    ImportOptions importOptions;
    const std::unordered_map<std::string, std::string> optionMap = dcc_import_policy::ParseOptionMap(options);
    importOptions.importSkin = dcc_import_policy::ParseBoolOption(optionMap, "importskin", true);
    importOptions.importMaterials = dcc_import_policy::ParseBoolOption(optionMap, "importmaterials", true);
    importOptions.importDeltaStates = dcc_import_policy::ParseBoolOption(optionMap, "importdeltastates", true);
    importOptions.applyLegacyAxisCorrection = dcc_import_policy::ParseBoolOption(optionMap, "applyaxiscorrection", false);
    importOptions.scenePolicy = dcc_import_policy::ParseSceneImportPolicy(optionMap);
    importOptions.transformCorrection = dcc_import_transform::ParseTransformCorrection(optionMap);
    return importOptions;
}

std::string ReadTextFile(const MString &filePath)
{
    std::ifstream file(filePath.asChar(), std::ios::in | std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::string NormalizeAxisName(std::string axisName)
{
    std::transform(axisName.begin(), axisName.end(), axisName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return axisName;
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

MMatrix BuildDmxTransformMatrix(const simple_dmx::Document &document, const simple_dmx::Element *dagElement, bool *hasTransform)
{
    if (hasTransform)
    {
        *hasTransform = false;
    }

    const simple_dmx::Element *transformElement = FindAttributeElement(document, dagElement, "transform");
    if (!transformElement)
    {
        return MMatrix::identity;
    }

    if (hasTransform)
    {
        *hasTransform = true;
    }

    const std::vector<double> positionValues = ParseNumberList(FindAttributeString(transformElement, "position"));
    const std::vector<double> orientationValues = ParseNumberList(FindAttributeString(transformElement, "orientation"));

    MTransformationMatrix transformMatrix;
    if (positionValues.size() >= 3)
    {
        transformMatrix.setTranslation(MVector(positionValues[0], positionValues[1], positionValues[2]), MSpace::kTransform);
    }

    if (orientationValues.size() >= 4)
    {
        const MQuaternion rotation(
            orientationValues[0],
            orientationValues[1],
            orientationValues[2],
            orientationValues[3]);
        transformMatrix.setRotationQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);
    }

    return transformMatrix.asMatrix();
}

// --- Material / node helpers ---

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
        return MStatus::kFailure;
    }
    status = plug.child(1).setFloat(components[1]);
    if (!status)
    {
        return MStatus::kFailure;
    }
    return plug.child(2).setFloat(components[2]);
}

MStatus ConnectPlugs(MPlug sourcePlug, MPlug destinationPlug)
{
    MDGModifier modifier;
    MStatus status = DisconnectDestinationPlug(modifier, destinationPlug);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = modifier.connect(sourcePlug, destinationPlug);
    if (!status)
    {
        return MStatus::kFailure;
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
        return MStatus::kFailure;
    }

    MFnDependencyNode fileNodeFn(fileNodeObject, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug fileTextureNamePlug = fileNodeFn.findPlug("fileTextureName", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }
    status = fileTextureNamePlug.setString(texturePath.c_str());
    if (!status)
    {
        return MStatus::kFailure;
    }

    MPlug outputPlug = fileNodeFn.findPlug(useAlphaOutput ? "outAlpha" : "outColor", true, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return ConnectPlugs(outputPlug, destinationPlug);
}

} // namespace dmx_import_impl

