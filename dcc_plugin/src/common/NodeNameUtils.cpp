#include "NodeNameUtils.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>

namespace dcc_node_name_detail
{

bool IsValidExportName(const std::string &exportName)
{
    return !exportName.empty();
}

} // namespace dcc_node_name_detail

bool dcc_node_name::TryReadExportNameOverride(const MObject &nodeObject, std::string &exportName)
{
    exportName.clear();

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug exportNamePlug = nodeFn.findPlug(kSourceExportNameAttribute, true, &status);
    if (!status || exportNamePlug.isNull())
    {
        return false;
    }

    exportName = exportNamePlug.asString(&status).asChar();
    if (!status || !dcc_node_name_detail::IsValidExportName(exportName))
    {
        exportName.clear();
        return false;
    }

    return true;
}

std::string dcc_node_name::ResolveExportNodeName(
    const MObject &nodeObject,
    const std::string &fallbackName,
    bool useExportNameOverride)
{
    if (!useExportNameOverride)
    {
        return fallbackName;
    }

    std::string exportName;
    if (TryReadExportNameOverride(nodeObject, exportName))
    {
        return exportName;
    }

    return fallbackName;
}

MStatus dcc_node_name::EnsureExportNameOverride(const MObject &nodeObject, const std::string &exportName)
{
    if (!dcc_node_name_detail::IsValidExportName(exportName))
    {
        return MS::kFailure;
    }

    MStatus status;
    MFnDependencyNode nodeFn(nodeObject, &status);
    if (!status)
    {
        return MS::kFailure;
    }

    MObject attributeObject = nodeFn.attribute(kSourceExportNameAttribute, &status);
    if (!status)
    {
        MFnTypedAttribute attributeFn;
        MFnStringData stringDataFn;
        MObject defaultValue = stringDataFn.create("", &status);
        if (!status)
        {
            return MS::kFailure;
        }

        attributeObject = attributeFn.create(
            kSourceExportNameAttribute,
            kSourceExportNameAttribute,
            MFnData::kString,
            defaultValue,
            &status);
        if (!status)
        {
            return MS::kFailure;
        }

        attributeFn.setReadable(true);
        attributeFn.setWritable(true);
        attributeFn.setStorable(true);
        attributeFn.setKeyable(false);

        status = nodeFn.addAttribute(attributeObject);
        if (!status)
        {
            return MS::kFailure;
        }
    }

    MPlug exportNamePlug = nodeFn.findPlug(kSourceExportNameAttribute, true, &status);
    if (!status || exportNamePlug.isNull())
    {
        return MS::kFailure;
    }

    status = exportNamePlug.setString(exportName.c_str());
    return status ? MS::kSuccess : MS::kFailure;
}
