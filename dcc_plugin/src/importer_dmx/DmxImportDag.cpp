#include "DmxImportDag.h"

#include "DmxImportMesh.h"

#include <common/ImportTransformCorrection.h>
#include <common/SceneMergeStrategy.h>

#include <string>

#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MItDag.h>
#include <maya/MStatus.h>
#include <maya/MTransformationMatrix.h>

namespace dmx_import_impl
{

MStatus ApplyTransform(
    const simple_dmx::Document &document,
    const simple_dmx::Element *dagElement,
    MObject object,
    const MMatrix &preTransform)
{
    bool hasTransform = false;
    const MMatrix localMatrix = BuildDmxTransformMatrix(document, dagElement, &hasTransform);
    if (!hasTransform && preTransform.isEquivalent(MMatrix::identity))
    {
        return MS::kSuccess;
    }

    const MTransformationMatrix correctedTransform(preTransform * localMatrix);
    const MVector translation = correctedTransform.getTranslation(MSpace::kTransform);
    MQuaternion rotation = correctedTransform.rotation();
    double scale[3] = {1.0, 1.0, 1.0};
    correctedTransform.getScale(scale, MSpace::kTransform);

    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = transformFn.setTranslation(translation, MSpace::kTransform);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = transformFn.setRotation(rotation);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = transformFn.setScale(scale);
    return status ? MS::kSuccess : MS::kFailure;
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
    const std::string rawNodeName = dagElement->name.empty() ? dagElement->type : dagElement->name;
    const std::string nodeName = SanitizeNodeName(rawNodeName);
    AppendImportDebugLog((std::string("dag: enter hierarchy type=") + dagElement->type + " raw=" + rawNodeName + " sanitized=" + nodeName).c_str());

    MStatus status;
    MObject nodeObject = MObject::kNullObj;
    const dcc_import_policy::SceneMergeResolver mergeResolver(context.scenePolicy);
    const bool reuseExistingMode = mergeResolver.usesExistingObjectMerge();
    const bool animationOnlyMode = mergeResolver.usesAnimationOnlyImport();
    if (reuseExistingMode)
    {
        nodeObject = mergeResolver.findAppendTargetChild(parent, nodeName, isJoint);
    }

    const bool reusedExistingNode = !nodeObject.isNull();
    AppendImportDebugLog((std::string("dag: hierarchy target reused=") + (reusedExistingNode ? "1" : "0")).c_str());
    if (animationOnlyMode && !reusedExistingNode)
    {
        AppendImportDebugLog((std::string("dag: animationOnly skip subtree missing existing node=") + nodeName).c_str());
        return MS::kSuccess;
    }

    if (!reusedExistingNode)
    {
        nodeObject = CreateDagNode(nodeName, isJoint, parent, status);
    }
    if (!status)
    {
        AppendImportDebugLog((std::string("dag: create node failed name=") + nodeName).c_str());
        return MStatus::kFailure;
    }
    if (!reusedExistingNode)
    {
        AppendImportDebugLog((std::string("dag: created node name=") + nodeName).c_str());
    }

    MDagPath nodePath;
    status = MDagPath::getAPathTo(nodeObject, nodePath);
    if (!status)
    {
        AppendImportDebugLog((std::string("dag: get path failed name=") + nodeName).c_str());
        return MStatus::kFailure;
    }
    AppendImportDebugLog((std::string("dag: path=") + nodePath.fullPathName().asChar()).c_str());
    context.importedDagPaths[ElementKey(dagElement)] = nodePath;
    if (reusedExistingNode)
    {
        context.reusedDagElementKeys.insert(ElementKey(dagElement));
    }
    if (const simple_dmx::Element *transformElement = FindAttributeElement(context.document, dagElement, "transform"))
    {
        context.importedTransformPaths[ElementKey(transformElement)] = nodePath;
        if (reusedExistingNode)
        {
            context.reusedTransformElementKeys.insert(ElementKey(transformElement));
        }
    }
    if (SanitizeNodeName(dagElement->name).size() >= 9 &&
        SanitizeNodeName(dagElement->name).rfind("_controls") == SanitizeNodeName(dagElement->name).size() - 9)
    {
        context.importedControlPaths.push_back(nodePath);
    }

    if (mergeResolver.shouldApplyBaseTransformToNode(reusedExistingNode))
    {
        const bool topLevelNode = parent == context.sceneRoot;
        AppendImportDebugLog((std::string("dag: apply transform name=") + nodeName + " topLevel=" + (topLevelNode ? "1" : "0")).c_str());
        status = ApplyTransform(
            context.document,
            dagElement,
            nodeObject,
            topLevelNode ? context.topLevelPreTransform : MMatrix::identity);
        if (!status)
        {
            AppendImportDebugLog((std::string("dag: apply transform failed name=") + nodeName).c_str());
            return MStatus::kFailure;
        }
        AppendImportDebugLog((std::string("dag: apply transform ok name=") + nodeName).c_str());
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        AppendImportDebugLog((std::string("dag: recurse child parent=") + nodeName + " child=" + (child ? child->name : "<null>")).c_str());
        status = ImportDagHierarchyRecursive(context, child, nodeObject);
        if (!status)
        {
            AppendImportDebugLog((std::string("dag: recurse child failed parent=") + nodeName).c_str());
            return MStatus::kFailure;
        }
    }

    AppendImportDebugLog((std::string("dag: leave hierarchy name=") + nodeName).c_str());
    return MS::kSuccess;
}

MStatus ImportDagShapesRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement)
{
    const dcc_import_policy::SceneMergeResolver mergeResolver(context.scenePolicy);
    if (!mergeResolver.shouldImportShapes())
    {
        return MS::kSuccess;
    }

    if (!dagElement)
    {
        return MS::kSuccess;
    }

    const std::string elementKey = ElementKey(dagElement);
    AppendImportDebugLog((std::string("dag: enter shapes element=") + dagElement->name + " type=" + dagElement->type).c_str());
    auto it = context.importedDagPaths.find(elementKey);
    if (it == context.importedDagPaths.end())
    {
        AppendImportDebugLog((std::string("dag: missing imported path for shapes element=") + dagElement->name).c_str());
        return maya_dmx::ReportError(MString("maya_dmx: imported DAG path was missing for ") + dagElement->name.c_str());
    }

    MStatus status;
    MObject nodeObject = it->second.node(&status);
    if (!status)
    {
        AppendImportDebugLog((std::string("dag: shape node lookup failed element=") + dagElement->name).c_str());
        return MStatus::kFailure;
    }

    AppendImportDebugLog((std::string("dag: create mesh shape begin element=") + dagElement->name).c_str());
    status = CreateMeshShape(context, dagElement, nodeObject);
    if (!status)
    {
        AppendImportDebugLog((std::string("dag: create mesh shape failed element=") + dagElement->name).c_str());
        return MStatus::kFailure;
    }
    AppendImportDebugLog((std::string("dag: create mesh shape ok element=") + dagElement->name).c_str());

    for (const simple_dmx::Element *child : FindAttributeElementArray(context.document, dagElement, "children"))
    {
        AppendImportDebugLog((std::string("dag: recurse shapes parent=") + dagElement->name + " child=" + (child ? child->name : "<null>")).c_str());
        status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            AppendImportDebugLog((std::string("dag: recurse shapes failed parent=") + dagElement->name).c_str());
            return MStatus::kFailure;
        }
    }

    AppendImportDebugLog((std::string("dag: leave shapes element=") + dagElement->name).c_str());
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

