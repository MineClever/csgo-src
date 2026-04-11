#include "DmxImportDag.h"

#include "DmxImportMesh.h"

#include <string>

#include <maya/MDagPath.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>

namespace dmx_import_impl
{


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
        return MStatus::kFailure;
    }

    if (positionValues.size() >= 3)
    {
        status = transformFn.setTranslation(MVector(positionValues[0], positionValues[1], positionValues[2]), MSpace::kTransform);
        if (!status)
        {
            return MStatus::kFailure;
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
            return MStatus::kFailure;
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
        return MStatus::kFailure;
    }

    MDagPath nodePath;
    status = MDagPath::getAPathTo(nodeObject, nodePath);
    if (!status)
    {
        return MStatus::kFailure;
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
        return MStatus::kFailure;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        status = ImportDagHierarchyRecursive(context, child, nodeObject);
        if (!status)
        {
            return MStatus::kFailure;
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
        return MStatus::kFailure;
    }

    status = CreateMeshShape(context, dagElement, nodeObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            return MStatus::kFailure;
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

} // namespace dmx_import_impl

