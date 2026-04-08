#include "DmxExportInternals.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

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

// --- Parsing ---

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

    std::vector<double> values;
    std::istringstream stream(normalized);
    double value = 0.0;
    while (stream >> value)
    {
        values.push_back(value);
    }

    return values;
}

// --- Formatting ---

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

    MString command("sculptTarget -e -regenerate true -target ");
    command += static_cast<int>(weightIndex);
    command += " \"";
    command += blendShapeNodeName;
    command += "\"";

    MStringArray result;
    if (MGlobal::executeCommand(command, result, false, false) != MS::kSuccess || result.length() == 0)
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

    MStringArray sourceConnections;
    MString command = "listConnections -s true -d false -plugs true \"";
    command += plug.name();
    command += "\"";
    if (MGlobal::executeCommand(command, sourceConnections, false, false) != MS::kSuccess)
    {
        return MObject::kNullObj;
    }

    for (unsigned int connectionIndex = 0; connectionIndex < sourceConnections.length(); ++connectionIndex)
    {
        MSelectionList selectionList;
        if (selectionList.add(sourceConnections[connectionIndex]) != MS::kSuccess)
        {
            continue;
        }

        MPlug sourcePlug;
        if (selectionList.getPlug(0, sourcePlug) != MS::kSuccess)
        {
            continue;
        }

        MStatus status;
        const MObject node = sourcePlug.node(&status);
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
