#include "DmxExportDag.h"

#include "DmxExportMesh.h"

#include <common/NodeNameUtils.h>

#include <string>
#include <vector>

#include <maya/MFnDagNode.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MVector.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>

namespace dmx_export_impl
{

static bool ShouldExportRoot(const MDagPath &dagPath)
{
    if (!dagPath.isValid())
    {
        return false;
    }

    MFnDagNode dagNode(dagPath);
    if (dagNode.isIntermediateObject())
    {
        return false;
    }

    return dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint);
}

bool IsTopLevelExportNode(const MDagPath &dagPath, const ExportContext &context)
{
    return dagPath.isValid() && context.topLevelDagPaths.find(DagPathKey(dagPath)) != context.topLevelDagPaths.end();
}

Element *BuildTransformElement(
    DocumentBuilder &builder,
    const MDagPath &dagPath,
    const ExportContext &context,
    bool isTopLevelNode)
{
    MStatus status;
    MFnTransform transformFn(dagPath, &status);
    if (!status)
    {
        return nullptr;
    }

    const MVector translation = transformFn.translation(MSpace::kTransform, &status);
    if (!status)
    {
        return nullptr;
    }

    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    double qw = 1.0;
    status = transformFn.getRotationQuaternion(qx, qy, qz, qw, MSpace::kTransform);
    if (!status)
    {
        return nullptr;
    }

    Element *transformElement = builder.CreateElement("DmeTransform");
    SetAttr(
        *transformElement,
        "position",
        ScalarAttr("vector3", FormatVector3(translation.x, translation.y, translation.z)));
    SetAttr(
        *transformElement,
        "orientation",
        ScalarAttr(
            "quaternion",
            FormatQuaternion(qx, qy, qz, qw)));
    return transformElement;
}

void RegisterDagElementsRecursive(DocumentBuilder &builder, const MDagPath &dagPath, ExportContext &context)
{
    if (!dagPath.isValid())
    {
        return;
    }

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status || dagNode.isIntermediateObject())
    {
        return;
    }

    if (!(dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint)))
    {
        return;
    }

    const std::string pathKey = DagPathKey(dagPath);
    auto dagElementIt = context.dagElementByPath.find(pathKey);
    if (dagElementIt == context.dagElementByPath.end())
    {
        const std::string elementType = dagPath.hasFn(MFn::kJoint) ? "DmeJoint" : "DmeDag";
        Element *dagElement = builder.CreateElement(elementType);
        context.dagElementByPath[pathKey] = dagElement;
        if (dagPath.hasFn(MFn::kJoint))
        {
            context.jointIndexByPath[pathKey] = static_cast<int>(context.jointElements.size());
            context.jointElements.push_back(dagElement);
        }
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        RegisterDagElementsRecursive(builder, childPath, context);
    }
}

std::vector<MDagPath> CollectExportRoots(MPxFileTranslator::FileAccessMode mode)
{
    std::vector<MDagPath> roots;

    if (mode == MPxFileTranslator::kExportActiveAccessMode)
    {
        MSelectionList activeSelection;
        if (MGlobal::getActiveSelectionList(activeSelection) == MS::kSuccess)
        {
            for (unsigned int i = 0; i < activeSelection.length(); ++i)
            {
                MDagPath dagPath;
                if (activeSelection.getDagPath(i, dagPath) != MS::kSuccess)
                {
                    continue;
                }

                if (dagPath.hasFn(MFn::kMesh))
                {
                    dagPath.pop();
                }

                if (ShouldExportRoot(dagPath))
                {
                    roots.push_back(dagPath);
                }
            }
        }
    }

    if (roots.empty())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            if (dagIterator.depth() != 1)
            {
                continue;
            }

            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) == MS::kSuccess && ShouldExportRoot(dagPath))
            {
                roots.push_back(dagPath);
            }
        }
    }

    std::vector<MDagPath> filteredRoots;
    for (const MDagPath &candidate : roots)
    {
        bool isDescendant = false;
        for (const MDagPath &other : roots)
        {
            if (candidate == other)
            {
                continue;
            }

            const MString candidatePath = candidate.fullPathName();
            const MString otherPath = other.fullPathName();
            if (candidate.length() > other.length() && candidatePath.indexW(otherPath) == 0)
            {
                isDescendant = true;
                break;
            }
        }

        if (!isDescendant)
        {
            filteredRoots.push_back(candidate);
        }
    }

    return filteredRoots;
}

Element *BuildDagElement(DocumentBuilder &builder, const MDagPath &dagPath, ExportContext &context)
{
    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status || dagNode.isIntermediateObject())
    {
        return nullptr;
    }

    const std::string pathKey = DagPathKey(dagPath);
    auto dagElementIt = context.dagElementByPath.find(pathKey);
    if (dagElementIt == context.dagElementByPath.end() || !dagElementIt->second)
    {
        return nullptr;
    }

    const std::string elementType = dagPath.hasFn(MFn::kJoint) ? "DmeJoint" : "DmeDag";
    Element *dagElement = dagElementIt->second;
    dagElement->type = elementType;
    ClearAttrs(*dagElement);
    dagElement->name = dcc_node_name::ResolveExportNodeName(
        dagPath.node(),
        dagNode.name().asChar(),
        context.useExportNameOverride);

    if (Element *transformElement = BuildTransformElement(
            builder,
            dagPath,
            context,
            IsTopLevelExportNode(dagPath, context)))
    {
        context.transformElementByPath[pathKey] = transformElement;
        SetAttr(*dagElement, "transform", builder.ElementRef(transformElement));
    }

    {
        MDagPath outputMeshPath;
        MDagPath bindShapeMeshPath;
        bool foundOutputMesh = false;
        bool foundBindShapeMesh = false;
        for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
        {
            MObject childObject = dagNode.child(childIndex, &status);
            if (!status || !childObject.hasFn(MFn::kMesh))
            {
                continue;
            }

            MDagPath meshPath = dagPath;
            meshPath.push(childObject);
            MFnDagNode meshDagNode(meshPath, &status);
            if (!status)
            {
                continue;
            }

            if (meshDagNode.isIntermediateObject())
            {
                if (!foundBindShapeMesh)
                {
                    bindShapeMeshPath = meshPath;
                    foundBindShapeMesh = true;
                }
            }
            else
            {
                if (!foundOutputMesh)
                {
                    outputMeshPath = meshPath;
                    foundOutputMesh = true;
                }
            }
        }

        if (foundOutputMesh)
        {
            const MDagPath *bindShapePtr = foundBindShapeMesh ? &bindShapeMeshPath : nullptr;
            if (Element *meshElement = BuildMeshElement(builder, outputMeshPath, context, bindShapePtr))
            {
                SetAttr(*dagElement, "shape", builder.ElementRef(meshElement));
            }
        }
    }

    std::vector<Element *> childElements;
    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        if (Element *childElement = BuildDagElement(builder, childPath, context))
        {
            childElements.push_back(childElement);
        }
    }

    if (!childElements.empty())
    {
        SetAttr(*dagElement, "children", builder.ElementRefArray(childElements));
    }

    return dagElement;
}

} // namespace dmx_export_impl
