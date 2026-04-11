#include "SmdSceneImporter.h"
#include "SmdMeshImporter.h"

#include "../common_smd/MayaSmdCommon.h"

#include <algorithm>
#include <string>

#include <maya/MAnimControl.h>
#include <maya/MAngle.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

namespace
{
std::string SanitizeNodeName(std::string value)
{
    for (char &character : value)
    {
        if (character == '|' || character == ':' || character == '"' || character == '\t' || character == '\r' || character == '\n')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("smd_node") : value;
}

MStatus SetPoseOnObject(MObject object, const simple_smd::SkeletonPose &pose)
{
    MStatus status;
    MFnTransform transformFn(object, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access transform for skeleton pose.", status);
    }

    status = transformFn.setTranslation(MVector(pose.tx, pose.ty, pose.tz), MSpace::kTransform);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton translation.", status);
    }

    status = transformFn.setRotation(MEulerRotation(pose.rx, pose.ry, pose.rz));
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply skeleton rotation.", status);
    }

    return MS::kSuccess;
}

MStatus SetCurveKeys(
    const MPlug &plug,
    const std::vector<double> &times,
    const std::vector<double> &values,
    MFnAnimCurve::AnimCurveType curveType)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return MS::kSuccess;
    }

    MStatus status;
    MFnAnimCurve curveFn;
    curveFn.create(plug, curveType, nullptr, &status);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to create animation curve for ") + plug.name(), status);
    }

    for (size_t index = 0; index < times.size(); ++index)
    {
        curveFn.addKey(
            MTime(times[index], MTime::uiUnit()),
            values[index],
            MFnAnimCurve::kTangentLinear,
            MFnAnimCurve::kTangentLinear,
            nullptr,
            &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to add animation key for ") + plug.name(), status);
        }
    }

    return MS::kSuccess;
}

MObject FindAppendTargetChild(const MObject &parent, const std::string &nodeName)
{
    MStatus status;
    if (parent.isNull())
    {
        MItDag dagIterator(MItDag::kDepthFirst);
        for (; !dagIterator.isDone(); dagIterator.next())
        {
            if (dagIterator.depth() != 1)
            {
                continue;
            }

            MDagPath dagPath;
            if (dagIterator.getPath(dagPath) != MS::kSuccess || !dagPath.hasFn(MFn::kJoint))
            {
                continue;
            }

            MFnDagNode dagNode(dagPath, &status);
            if (status && std::string(dagNode.name().asChar()) == nodeName)
            {
                return dagPath.node();
            }
        }

        return MObject::kNullObj;
    }

    MFnDagNode parentDagNode(parent, &status);
    if (!status)
    {
        return MObject::kNullObj;
    }

    for (unsigned int childIndex = 0; childIndex < parentDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = parentDagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kJoint))
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDagNode childDagNode(childObject, &status);
        if (status && std::string(childDagNode.name().asChar()) == nodeName)
        {
            return childObject;
        }
        status = MS::kSuccess;
    }

    return MObject::kNullObj;
}
}

SmdSceneImporter::SmdSceneImporter(std::shared_ptr<const simple_smd::Document> document, const SmdImportOptions &importOptions)
    : document_(document)
    , importOptions_(importOptions)
{
}

MStatus SmdSceneImporter::Import()
{
    MStatus status = createImportRoot();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = applyImportRotation();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = createJointHierarchy();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = applyBindPose();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = applyAnimation();
    if (!status)
    {
        return MStatus::kFailure;
    }

    auto jointPathsByBonePtr = std::shared_ptr<const std::unordered_map<int, MDagPath>>(
        &jointPathsByBone_,
        [](const std::unordered_map<int, MDagPath> *) {});
    SmdMeshImporter meshImporter(document_, jointPathsByBonePtr, importOptions_.scenePolicy);
    status = meshImporter.Import(importRoot_);
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (document_->hasVertexAnimation)
    {
        maya_smd::ReportWarning("maya_smd: vertexanimation section detected but not implemented yet");
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createImportRoot()
{
    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy))
    {
        importRoot_ = MObject::kNullObj;
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform rootTransformFn;
    importRoot_ = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to create SMD import root.", status);
    }

    rootTransformFn.setName("smd_import_root#");

    return MS::kSuccess;
}

MStatus SmdSceneImporter::applyImportRotation()
{
    if (importOptions_.rotateXDegrees == 0.0
        && importOptions_.rotateYDegrees == 0.0
        && importOptions_.rotateZDegrees == 0.0)
    {
        return MS::kSuccess;
    }

    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy))
    {
        return maya_smd::ReportWarning("maya_smd: skipped custom import rotation because useSceneRoot=1 imports directly into the Maya scene.");
    }

    MStatus status;
    MFnTransform rootTransformFn(importRoot_, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access SMD import root for custom rotation.", status);
    }

    const MEulerRotation importRotation(
        MAngle(importOptions_.rotateXDegrees, MAngle::kDegrees).asRadians(),
        MAngle(importOptions_.rotateYDegrees, MAngle::kDegrees).asRadians(),
        MAngle(importOptions_.rotateZDegrees, MAngle::kDegrees).asRadians(),
        MEulerRotation::kXYZ);
    status = rootTransformFn.setRotation(importRotation);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to apply custom import rotation to SMD root.", status);
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createJointHierarchy()
{
    for (const simple_smd::Node &node : document_->nodes)
    {
        const MStatus status = createJoint(node);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::createJoint(const simple_smd::Node &node)
{
    MStatus status;
    const MObject parentObject = findParentObject(node);
    MObject jointObject = MObject::kNullObj;
    const bool appendMissingMode = dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy);
    if (appendMissingMode)
    {
        jointObject = findExistingJoint(node);
    }

    const bool reusedExistingJoint = !jointObject.isNull();
    if (!reusedExistingJoint)
    {
        MFnIkJoint jointFn;
        jointObject = jointFn.create(parentObject, &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to create joint for node ") + node.name.c_str(), status);
        }

        jointFn.setName(SanitizeNodeName(node.name).c_str());
    }
    else
    {
        reusedBoneIndices_.insert(node.index);
    }

    MDagPath jointPath;
    status = MDagPath::getAPathTo(jointObject, jointPath);
    if (!status)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to resolve DAG path for joint ") + node.name.c_str(), status);
    }

    jointPathsByBone_[node.index] = jointPath;
    return MS::kSuccess;
}

MObject SmdSceneImporter::findExistingJoint(const simple_smd::Node &node) const
{
    return FindAppendTargetChild(findParentObject(node), SanitizeNodeName(node.name));
}

MObject SmdSceneImporter::findParentObject(const simple_smd::Node &node) const
{
    auto parentIt = jointPathsByBone_.find(node.parentIndex);
    if (parentIt == jointPathsByBone_.end())
    {
        return importRoot_;
    }

    return parentIt->second.node();
}

const simple_smd::SkeletonPose *SmdSceneImporter::findPose(const simple_smd::SkeletonFrame &frame, int boneIndex) const
{
    const auto poseIt = std::find_if(
        frame.poses.begin(),
        frame.poses.end(),
        [boneIndex](const simple_smd::SkeletonPose &pose)
        {
            return pose.boneIndex == boneIndex;
        });
    return poseIt == frame.poses.end() ? nullptr : &(*poseIt);
}

MStatus SmdSceneImporter::applyBindPose()
{
    if (document_->skeletonFrames.empty())
    {
        return MS::kSuccess;
    }

    const simple_smd::SkeletonFrame &bindFrame = document_->skeletonFrames.front();
    for (const simple_smd::Node &node : document_->nodes)
    {
        if (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy) &&
            reusedBoneIndices_.find(node.index) != reusedBoneIndices_.end())
        {
            continue;
        }

        const simple_smd::SkeletonPose *pose = findPose(bindFrame, node.index);
        if (!pose)
        {
            continue;
        }

        const auto pathIt = jointPathsByBone_.find(node.index);
        if (pathIt == jointPathsByBone_.end())
        {
            continue;
        }

        const MStatus status = SetPoseOnObject(pathIt->second.node(), *pose);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MS::kSuccess;
}

MStatus SmdSceneImporter::applyAnimation()
{
    if (document_->skeletonFrames.size() <= 1)
    {
        return MS::kSuccess;
    }

    for (const simple_smd::Node &node : document_->nodes)
    {
        if (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy) &&
            reusedBoneIndices_.find(node.index) != reusedBoneIndices_.end())
        {
            continue;
        }

        const auto pathIt = jointPathsByBone_.find(node.index);
        if (pathIt == jointPathsByBone_.end())
        {
            continue;
        }

        MStatus status;
        MFnDependencyNode dependencyNodeFn(pathIt->second.node(), &status);
        if (!status)
        {
            return maya_smd::ReportError(MString("maya_smd: failed to access dependency node for animated joint ") + node.name.c_str(), status);
        }

        std::vector<double> times;
        std::vector<double> txValues;
        std::vector<double> tyValues;
        std::vector<double> tzValues;
        std::vector<double> rxValues;
        std::vector<double> ryValues;
        std::vector<double> rzValues;

        for (const simple_smd::SkeletonFrame &frame : document_->skeletonFrames)
        {
            const simple_smd::SkeletonPose *pose = findPose(frame, node.index);
            if (!pose)
            {
                continue;
            }

            times.push_back(static_cast<double>(frame.time));
            txValues.push_back(pose->tx);
            tyValues.push_back(pose->ty);
            tzValues.push_back(pose->tz);
            rxValues.push_back(pose->rx);
            ryValues.push_back(pose->ry);
            rzValues.push_back(pose->rz);
        }

        if (times.size() <= 1)
        {
            continue;
        }

        const MPlug translateXPlug = dependencyNodeFn.findPlug("translateX", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateX plug for animated joint.", status);
        }
        status = SetCurveKeys(translateXPlug, times, txValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateY plug for animated joint.", status);
        }
        const MPlug translateYPlug = dependencyNodeFn.findPlug("translateY", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find translateZ plug for animated joint.", status);
        }
        status = SetCurveKeys(translateYPlug, times, tyValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateX plug for animated joint.", status);
        }
        const MPlug translateZPlug = dependencyNodeFn.findPlug("translateZ", true, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateY plug for animated joint.", status);
        }
        status = SetCurveKeys(translateZPlug, times, tzValues, MFnAnimCurve::kAnimCurveTL);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to find rotateZ plug for animated joint.", status);
        }
        const MPlug rotateXPlug = dependencyNodeFn.findPlug("rotateX", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = SetCurveKeys(rotateXPlug, times, rxValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
        const MPlug rotateYPlug = dependencyNodeFn.findPlug("rotateY", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = SetCurveKeys(rotateYPlug, times, ryValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
        const MPlug rotateZPlug = dependencyNodeFn.findPlug("rotateZ", true, &status);
        if (!status)
        {
            return MStatus::kFailure;
        }
        status = SetCurveKeys(rotateZPlug, times, rzValues, MFnAnimCurve::kAnimCurveTA);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    MAnimControl::setMinTime(MTime(static_cast<double>(document_->skeletonFrames.front().time), MTime::uiUnit()));
    MAnimControl::setMaxTime(MTime(static_cast<double>(document_->skeletonFrames.back().time), MTime::uiUnit()));
    MAnimControl::setCurrentTime(MTime(static_cast<double>(document_->skeletonFrames.front().time), MTime::uiUnit()));
    return MS::kSuccess;
}
