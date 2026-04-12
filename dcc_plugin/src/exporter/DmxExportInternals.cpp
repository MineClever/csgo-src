#include "DmxExportInternals.h"

#include <common/MayaCommandUtils.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <Windows.h>

#include <maya/MFnAnimCurve.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MGlobal.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>

namespace dmx_export_impl
{

// --- Private helpers (not declared in DmxExportInternals.h) ---

static MObject FindPrimaryMeshChild(const MObject &nodeObject)
{
    if (nodeObject.isNull())
    {
        return MObject::kNullObj;
    }

    if (nodeObject.hasFn(MFn::kMesh))
    {
        return nodeObject;
    }

    if (!nodeObject.hasFn(MFn::kTransform))
    {
        return MObject::kNullObj;
    }

    MStatus status;
    MFnDagNode dagNode(nodeObject, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject child = dagNode.child(childIndex, &status);
        if (!status || child.isNull())
        {
            continue;
        }

        if (child.hasFn(MFn::kMesh))
        {
            MFnDagNode childDagNode(child, &status);
            if (status && !childDagNode.isIntermediateObject())
            {
                return child;
            }
        }
    }

    return MObject::kNullObj;
}

static void AppendUniqueTime(std::vector<double> &times, double value)
{
    const auto it = std::lower_bound(times.begin(), times.end(), value);
    if (it != times.end() && std::abs(*it - value) < 1.0e-6)
    {
        return;
    }
    if (it != times.begin())
    {
        const auto previous = it - 1;
        if (std::abs(*previous - value) < 1.0e-6)
        {
            return;
        }
    }
    times.insert(it, value);
}

// --- Formatting ---

void AppendDebugLog(const char *message)
{
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }

    std::string logPath(tempPath);
    logPath += "maya_dmx_export_debug.log";

    std::ofstream logFile(logPath.c_str(), std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        return;
    }

    logFile << message << "\n";
}

bool IsBinaryExportRequested(const MFileObject &fileObject, const MString &options)
{
    const std::string lowerPath = fileObject.rawFullName().toLowerCase().asChar();
    if (lowerPath.size() >= 5 && lowerPath.substr(lowerPath.size() - 5) == ".dmxb")
    {
        return true;
    }
    if (lowerPath.size() >= 7 && lowerPath.substr(lowerPath.size() - 7) == ".dmxbin")
    {
        return true;
    }

    std::string lowerOptions = options.asChar();
    std::transform(lowerOptions.begin(), lowerOptions.end(), lowerOptions.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowerOptions.find("binary=1") != std::string::npos ||
        lowerOptions.find("binary=true") != std::string::npos ||
        lowerOptions.find("encoding=binary") != std::string::npos;
}

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

ExportOptions ParseExportOptions(const MFileObject &fileObject, const MString &options)
{
    ExportOptions exportOptions;
    exportOptions.binary = IsBinaryExportRequested(fileObject, options);

    const std::unordered_map<std::string, std::string> optionMap = ParseOptionMap(options);
    exportOptions.exportSkin = ParseBoolOption(optionMap, "exportskin", true);
    exportOptions.exportDeltaStates = ParseBoolOption(optionMap, "exportdeltastates", true);
    exportOptions.exportMetadata = ParseBoolOption(optionMap, "exportmetadata", true);

    auto upAxisIt = optionMap.find("upaxis");
    if (upAxisIt != optionMap.end() && !upAxisIt->second.empty())
    {
        exportOptions.upAxis = upAxisIt->second;
    }

    auto materialRootIt = optionMap.find("materialroot");
    if (materialRootIt != optionMap.end())
    {
        exportOptions.materialRoot = materialRootIt->second;
    }

    return exportOptions;
}

std::string FormatFloat(double value)
{
    if (!std::isfinite(value))
    {
        value = 0.0;
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str();
}

std::string FormatVector2(double x, double y)
{
    return FormatFloat(x) + " " + FormatFloat(y);
}

std::string FormatVector3(double x, double y, double z)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z);
}

std::string FormatVector4(double x, double y, double z, double w)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z) + " " + FormatFloat(w);
}

std::string FormatQuaternion(double x, double y, double z, double w)
{
    return FormatFloat(x) + " " + FormatFloat(y) + " " + FormatFloat(z) + " " + FormatFloat(w);
}

std::string FormatMatrix(const MMatrix &matrix)
{
    std::ostringstream stream;
    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            if (row != 0 || column != 0)
            {
                stream << ' ';
            }
            stream << FormatFloat(matrix[row][column]);
        }
    }
    return stream.str();
}

std::string FormatTimeSeconds(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(4);
    stream << value;
    return stream.str();
}

// --- Plug / node helpers ---

std::string ReadStringPlugValue(const MPlug &plug)
{
    MString value;
    if (plug.getValue(value) == MS::kSuccess)
    {
        return value.asChar();
    }

    return std::string();
}

bool ReadVector3PlugValue(const MPlug &plug, std::string &formattedValue)
{
    if (plug.numChildren() < 3)
    {
        return false;
    }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (plug.child(0).getValue(x) != MS::kSuccess ||
        plug.child(1).getValue(y) != MS::kSuccess ||
        plug.child(2).getValue(z) != MS::kSuccess)
    {
        return false;
    }

    formattedValue = FormatVector3(x, y, z);
    return true;
}

std::string ReadMatrixPlugValue(const MPlug &plug)
{
    MObject matrixObject;
    if (plug.getValue(matrixObject) != MS::kSuccess || matrixObject.isNull())
    {
        return std::string();
    }

    MStatus status;
    MFnMatrixData matrixDataFn(matrixObject, &status);
    if (!status)
    {
        return std::string();
    }

    return FormatMatrix(matrixDataFn.matrix(&status));
}

MObject FindConnectedSourceNode(const MPlug &destinationPlug)
{
    MPlugArray connectedPlugs;
    if (destinationPlug.connectedTo(connectedPlugs, true, false) != MS::kSuccess || connectedPlugs.length() == 0)
    {
        return MObject::kNullObj;
    }

    for (unsigned int i = 0; i < connectedPlugs.length(); ++i)
    {
        const MObject node = connectedPlugs[i].node();
        if (!node.isNull())
        {
            return node;
        }
    }

    return MObject::kNullObj;
}

std::string FindTexturePathFromPlug(const MPlug &plug)
{
    const MObject sourceNode = FindConnectedSourceNode(plug);
    if (sourceNode.isNull())
    {
        return std::string();
    }

    MStatus status;
    MFnDependencyNode sourceNodeFn(sourceNode, &status);
    if (!status)
    {
        return std::string();
    }

    if (sourceNodeFn.typeName() == "file")
    {
        MPlug textureNamePlug = sourceNodeFn.findPlug("fileTextureName", true, &status);
        if (status)
        {
            return ReadStringPlugValue(textureNamePlug);
        }
        return std::string();
    }

    if (sourceNodeFn.typeName() == "bump2d" || sourceNodeFn.typeName() == "bump3d")
    {
        MPlug bumpPlug = sourceNodeFn.findPlug("bumpValue", true, &status);
        if (status)
        {
            return FindTexturePathFromPlug(bumpPlug);
        }
    }

    return std::string();
}

// --- DAG / mesh helpers ---

std::string DagPathKey(const MDagPath &dagPath)
{
    return dagPath.fullPathName().asChar();
}

bool TryGetMeshPathFromObject(const MObject &nodeObject, MDagPath &meshPath)
{
    MStatus status;
    MObject meshObject = FindPrimaryMeshChild(nodeObject);
    if (meshObject.isNull())
    {
        return false;
    }

    status = MDagPath::getAPathTo(meshObject, meshPath);
    return status == MS::kSuccess;
}

bool TryRegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MDagPath &targetPath,
    MString &temporaryTransformName)
{
    temporaryTransformName.clear();

    MStringArray result;
    if (maya_cmd::RegenerateBlendShapeTarget(blendShapeNodeName, weightIndex, result) != MS::kSuccess || result.length() == 0)
    {
        return false;
    }

    temporaryTransformName = result[0];

    MSelectionList selectionList;
    if (selectionList.add(temporaryTransformName) != MS::kSuccess)
    {
        return false;
    }

    MObject temporaryObject;
    if (selectionList.getDependNode(0, temporaryObject) != MS::kSuccess)
    {
        return false;
    }

    return TryGetMeshPathFromObject(temporaryObject, targetPath);
}

// --- Animation curve helpers ---

MObject FindAnimationCurveForPlug(const MPlug &plug)
{
    if (plug.isNull())
    {
        return MObject::kNullObj;
    }

    MPlugArray sourcePlugs;
    plug.connectedTo(sourcePlugs, true, false);

    for (unsigned int i = 0; i < sourcePlugs.length(); ++i)
    {
        MStatus status;
        const MObject node = sourcePlugs[i].node(&status);
        if (status && !node.isNull() && node.hasFn(MFn::kAnimCurve))
        {
            return node;
        }
    }

    return MObject::kNullObj;
}

void AppendCurveTimes(const MObject &curveObject, std::vector<double> &times)
{
    if (curveObject.isNull())
    {
        return;
    }

    MStatus status;
    MFnAnimCurve curveFn(curveObject, &status);
    if (!status)
    {
        return;
    }

    for (unsigned int keyIndex = 0; keyIndex < curveFn.numKeys(&status); ++keyIndex)
    {
        if (!status)
        {
            break;
        }

        AppendUniqueTime(times, curveFn.time(keyIndex, &status).as(MTime::kSeconds));
    }
}

double EvaluateCurveOrValue(const MObject &curveObject, const MPlug &plug, double timeSeconds)
{
    if (!curveObject.isNull())
    {
        MStatus status;
        MFnAnimCurve curveFn(curveObject, &status);
        if (status)
        {
            return curveFn.evaluate(MTime(timeSeconds, MTime::kSeconds), &status);
        }
    }

    double value = 0.0;
    plug.getValue(value);
    return value;
}

} // namespace dmx_export_impl
