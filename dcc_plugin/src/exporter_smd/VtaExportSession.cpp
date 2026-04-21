#include "VtaExportSession.h"

#include <common/MayaCommandUtils.h>
#include <common/TransformCorrection.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

namespace vta_export_session_detail
{
constexpr const char *kSmdRawVertexMapAttribute = "mayaSmdRawVertexMap";
constexpr const char *kVtaAliasPrefix = "vta_frame_";

bool ParseRawVertexMap(const MString &serializedMap, std::vector<int> &rawToLocalVertexIndex)
{
    rawToLocalVertexIndex.clear();

    std::istringstream stream(serializedMap.asChar());
    int rawIndex = -1;
    int localIndex = -1;
    while (stream >> rawIndex >> localIndex)
    {
        if (rawIndex < 0 || localIndex < 0)
        {
            return false;
        }

        if (static_cast<size_t>(rawIndex) >= rawToLocalVertexIndex.size())
        {
            rawToLocalVertexIndex.resize(static_cast<size_t>(rawIndex) + 1, -1);
        }
        rawToLocalVertexIndex[static_cast<size_t>(rawIndex)] = localIndex;
    }

    return !rawToLocalVertexIndex.empty();
}

bool LoadRawVertexMap(const MObject &meshObject, std::vector<int> &rawToLocalVertexIndex)
{
    rawToLocalVertexIndex.clear();

    MStatus status;
    MFnDependencyNode meshNode(meshObject, &status);
    if (!status)
    {
        return false;
    }

    MPlug mappingPlug = meshNode.findPlug(kSmdRawVertexMapAttribute, true, &status);
    if (!status || mappingPlug.isNull())
    {
        return false;
    }

    return ParseRawVertexMap(mappingPlug.asString(), rawToLocalVertexIndex);
}

void CollectSelectedMeshBindingsRecursive(const MDagPath &candidatePath, std::vector<VtaExportMeshBinding> &bindings)
{
    MStatus status;

    if (candidatePath.node().hasFn(MFn::kMesh))
    {
        MFnDagNode meshDagNode(candidatePath.node(), &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            VtaExportMeshBinding binding;
            binding.meshPath = candidatePath;
            binding.transformPath = candidatePath;
            binding.transformPath.pop();
            bindings.push_back(binding);
        }
        return;
    }

    if (!candidatePath.node().hasFn(MFn::kTransform) || candidatePath.node().hasFn(MFn::kJoint))
    {
        return;
    }

    MFnDagNode transformDagNode(candidatePath.node(), &status);
    if (!status)
    {
        return;
    }

    for (unsigned int childIndex = 0; childIndex < transformDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = transformDagNode.child(childIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        MDagPath childPath = candidatePath;
        childPath.push(childObject);
        CollectSelectedMeshBindingsRecursive(childPath, bindings);
    }
}

bool ParseFrameTimeFromAlias(const MString &aliasName, int &frameTime)
{
    frameTime = 0;
    const std::string alias = aliasName.asChar();
    const std::string prefix(kVtaAliasPrefix);
    if (alias.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    try
    {
        frameTime = std::stoi(alias.substr(prefix.size()));
        return frameTime >= 0;
    }
    catch (...)
    {
        return false;
    }
}

bool TryGetMeshPathFromObject(const MObject &meshObject, MDagPath &meshPath)
{
    if (meshObject.isNull())
    {
        return false;
    }

    MStatus status = MDagPath::getAPathTo(meshObject, meshPath);
    if (!status || !meshPath.isValid())
    {
        return false;
    }

    if (meshPath.node().hasFn(MFn::kMesh))
    {
        return true;
    }

    MFnDagNode dagNode(meshPath.node(), &status);
    if (!status)
    {
        return false;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        const MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            status = MS::kSuccess;
            continue;
        }

        MDagPath childPath = meshPath;
        childPath.push(childObject);
        meshPath = childPath;
        return true;
    }

    return false;
}

bool TryRegenerateBlendShapeTarget(
    const MString &blendShapeNodeName,
    unsigned int weightIndex,
    MDagPath &targetMeshPath,
    MString &temporaryTargetTransform)
{
    temporaryTargetTransform.clear();
    targetMeshPath = MDagPath();

    MStringArray regenerateResult;
    MStatus status = maya_cmd::RegenerateBlendShapeTarget(blendShapeNodeName, weightIndex, regenerateResult);
    if (!status || regenerateResult.length() == 0)
    {
        return false;
    }

    temporaryTargetTransform = regenerateResult[0];

    MSelectionList selection;
    status = selection.add(temporaryTargetTransform);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(temporaryTargetTransform);
        temporaryTargetTransform.clear();
        return false;
    }

    MDagPath transformPath;
    status = selection.getDagPath(0, transformPath);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(temporaryTargetTransform);
        temporaryTargetTransform.clear();
        return false;
    }

    MObject targetMeshObject;
    MFnDagNode transformDagNode(transformPath.node(), &status);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(temporaryTargetTransform);
        temporaryTargetTransform.clear();
        return false;
    }

    for (unsigned int childIndex = 0; childIndex < transformDagNode.childCount(); ++childIndex)
    {
        const MObject childObject = transformDagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDagNode meshDagNode(childObject, &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            targetMeshObject = childObject;
            break;
        }
        status = MS::kSuccess;
    }

    if (targetMeshObject.isNull() || !TryGetMeshPathFromObject(targetMeshObject, targetMeshPath))
    {
        maya_cmd::DeleteNodeByName(temporaryTargetTransform);
        temporaryTargetTransform.clear();
        return false;
    }

    return true;
}

MStatus CollectBlendShapeTargets(VtaExportMeshBinding &binding)
{
    binding.targets.clear();

    MStatus status;
    MObject meshNodeObject = binding.meshPath.node();
    MItDependencyGraph dependencyIt(
        meshNodeObject,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    for (; !dependencyIt.isDone(); dependencyIt.next())
    {
        const MObject blendShapeObject = dependencyIt.currentItem(&status);
        if (!status || blendShapeObject.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn(blendShapeObject, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        const MString blendShapeNodeName = blendShapeNodeFn.name();
        MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        MIntArray weightIndices;
        status = blendShapeFn.weightIndexList(weightIndices);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        for (unsigned int index = 0; index < weightIndices.length(); ++index)
        {
            const unsigned int weightIndex = static_cast<unsigned int>(weightIndices[index]);
            MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(weightIndex, &status);
            if (!status || weightPlug.isNull())
            {
                status = MS::kSuccess;
                continue;
            }

            const MString aliasName = blendShapeNodeFn.plugsAlias(weightPlug);
            int frameTime = 0;
            if (!ParseFrameTimeFromAlias(aliasName, frameTime))
            {
                continue;
            }

            VtaExportTarget target;
            target.blendShapeObject = blendShapeObject;
            target.blendShapeNodeName = blendShapeNodeName;
            target.weightIndex = weightIndex;
            target.frameTime = frameTime;
            binding.targets.push_back(target);
        }
    }

    std::sort(
        binding.targets.begin(),
        binding.targets.end(),
        [](const VtaExportTarget &lhs, const VtaExportTarget &rhs) {
            if (lhs.frameTime != rhs.frameTime)
            {
                return lhs.frameTime < rhs.frameTime;
            }
            if (lhs.blendShapeNodeName != rhs.blendShapeNodeName)
            {
                return std::string(lhs.blendShapeNodeName.asChar()) < std::string(rhs.blendShapeNodeName.asChar());
            }
            return lhs.weightIndex < rhs.weightIndex;
        });

    return MS::kSuccess;
}

void AppendAllBaseSamples(
    const VtaExportMeshBinding &binding,
    simple_smd::VertexAnimationFrame &frame)
{
    for (size_t rawIndex = 0; rawIndex < binding.rawToLocalVertexIndex.size(); ++rawIndex)
    {
        const int localVertexIndex = binding.rawToLocalVertexIndex[rawIndex];
        if (localVertexIndex < 0)
        {
            continue;
        }

        const unsigned int localIndex = static_cast<unsigned int>(localVertexIndex);
        if (localIndex >= binding.basePoints.length() || localIndex >= binding.baseNormals.length())
        {
            continue;
        }

        simple_smd::VertexAnimationSample sample;
        sample.vertexIndex = static_cast<int>(rawIndex);
        sample.px = binding.basePoints[localIndex].x;
        sample.py = binding.basePoints[localIndex].y;
        sample.pz = binding.basePoints[localIndex].z;
        sample.nx = binding.baseNormals[localIndex].x;
        sample.ny = binding.baseNormals[localIndex].y;
        sample.nz = binding.baseNormals[localIndex].z;
        frame.samples.push_back(sample);
    }
}

void AppendChangedTargetSamples(
    const VtaExportMeshBinding &binding,
    const MPointArray &targetPoints,
    const MFloatVectorArray &targetNormals,
    simple_smd::VertexAnimationFrame &frame)
{
    constexpr double kPointEpsilon = 1.0e-6;
    constexpr double kNormalEpsilon = 1.0e-6;

    for (size_t rawIndex = 0; rawIndex < binding.rawToLocalVertexIndex.size(); ++rawIndex)
    {
        const int localVertexIndex = binding.rawToLocalVertexIndex[rawIndex];
        if (localVertexIndex < 0)
        {
            continue;
        }

        const unsigned int localIndex = static_cast<unsigned int>(localVertexIndex);
        if (localIndex >= binding.basePoints.length() || localIndex >= binding.baseNormals.length() ||
            localIndex >= targetPoints.length() || localIndex >= targetNormals.length())
        {
            continue;
        }

        const MPoint basePoint = binding.basePoints[localIndex];
        const MFloatVector baseNormal = binding.baseNormals[localIndex];
        const MPoint targetPoint = targetPoints[localIndex];
        const MFloatVector targetNormal = targetNormals[localIndex];

        const bool pointChanged =
            std::abs(targetPoint.x - basePoint.x) > kPointEpsilon ||
            std::abs(targetPoint.y - basePoint.y) > kPointEpsilon ||
            std::abs(targetPoint.z - basePoint.z) > kPointEpsilon;
        const bool normalChanged =
            std::abs(targetNormal.x - baseNormal.x) > kNormalEpsilon ||
            std::abs(targetNormal.y - baseNormal.y) > kNormalEpsilon ||
            std::abs(targetNormal.z - baseNormal.z) > kNormalEpsilon;

        if (!pointChanged && !normalChanged)
        {
            continue;
        }

        simple_smd::VertexAnimationSample sample;
        sample.vertexIndex = static_cast<int>(rawIndex);
        sample.px = targetPoint.x;
        sample.py = targetPoint.y;
        sample.pz = targetPoint.z;
        sample.nx = targetNormal.x;
        sample.ny = targetNormal.y;
        sample.nz = targetNormal.z;
        frame.samples.push_back(sample);
    }
}
} // namespace vta_export_session_detail

using namespace vta_export_session_detail;

VtaExportSession::VtaExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , options_(options)
    , mode_(mode)
{
}

MStatus VtaExportSession::Run()
{
    const MStatus validationStatus = validateOutputFile();
    if (!validationStatus)
    {
        return MStatus::kFailure;
    }

    MStatus status = buildDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = serialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = writeOutput();
    if (!status)
    {
        return MStatus::kFailure;
    }

    return maya_smd::ReportInfo(MString("maya_smd: exported VTA to ") + fileObject_.rawFullName());
}

MStatus VtaExportSession::validateOutputFile() const
{
    if (!maya_smd::HasVtaExtension(fileObject_))
    {
        return maya_smd::ReportError(MString("maya_smd: unsupported VTA export extension for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

MStatus VtaExportSession::buildDocument()
{
    document_ = simple_smd::Document();
    document_.version = 1;

    SmdSceneExporter skeletonExporter(
        mode_,
        dcc_export_transform::ExportTransformPolicy(),
        false,
        true,
        true,
        false);
    MStatus status = skeletonExporter.Build();
    if (!status)
    {
        return MStatus::kFailure;
    }

    document_.nodes = skeletonExporter.document().nodes;
    document_.skeletonFrames = skeletonExporter.document().skeletonFrames;

    std::vector<VtaExportMeshBinding> bindings;
    status = collectMeshBindings(bindings);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return appendVertexAnimationFrames(bindings);
}

MStatus VtaExportSession::collectMeshBindings(std::vector<VtaExportMeshBinding> &bindings) const
{
    bindings.clear();

    MSelectionList activeSelection;
    MGlobal::getActiveSelectionList(activeSelection);
    if (activeSelection.length() == 0)
    {
        return maya_smd::ReportError("maya_smd: VTA export requires selecting the target mesh, meshes, or import root.");
    }

    for (unsigned int selectionIndex = 0; selectionIndex < activeSelection.length(); ++selectionIndex)
    {
        MDagPath selectedPath;
        MStatus status = activeSelection.getDagPath(selectionIndex, selectedPath);
        if (!status || !selectedPath.isValid())
        {
            continue;
        }

        CollectSelectedMeshBindingsRecursive(selectedPath, bindings);
    }

    std::sort(
        bindings.begin(),
        bindings.end(),
        [](const VtaExportMeshBinding &lhs, const VtaExportMeshBinding &rhs) {
            return std::string(lhs.meshPath.fullPathName().asChar()) <
                std::string(rhs.meshPath.fullPathName().asChar());
        });

    bindings.erase(
        std::unique(
            bindings.begin(),
            bindings.end(),
            [](const VtaExportMeshBinding &lhs, const VtaExportMeshBinding &rhs) {
                return lhs.meshPath.fullPathName() == rhs.meshPath.fullPathName();
            }),
        bindings.end());

    if (bindings.empty())
    {
        return maya_smd::ReportError("maya_smd: VTA export could not find selected meshes to export.");
    }

    bool foundAnyRawVertexMap = false;
    bool foundAnyTarget = false;
    for (VtaExportMeshBinding &binding : bindings)
    {
        if (!LoadRawVertexMap(binding.meshPath.node(), binding.rawToLocalVertexIndex))
        {
            continue;
        }

        foundAnyRawVertexMap = true;

        MStatus status;
        MFnMesh sourceMeshFn(binding.meshPath, &status);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to bind mesh function set for VTA export.", status);
        }

        status = sourceMeshFn.getPoints(binding.basePoints, MSpace::kObject);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to read source mesh points for VTA export.", status);
        }

        status = sourceMeshFn.getVertexNormals(false, binding.baseNormals, MSpace::kObject);
        if (!status)
        {
            return maya_smd::ReportError("maya_smd: failed to read source mesh normals for VTA export.", status);
        }

        status = CollectBlendShapeTargets(binding);
        if (!status)
        {
            return MStatus::kFailure;
        }

        if (!binding.targets.empty())
        {
            foundAnyTarget = true;
        }
    }

    bindings.erase(
        std::remove_if(
            bindings.begin(),
            bindings.end(),
            [](const VtaExportMeshBinding &binding) {
                return binding.rawToLocalVertexIndex.empty();
            }),
        bindings.end());

    if (!foundAnyRawVertexMap)
    {
        return maya_smd::ReportError(
            "maya_smd: VTA export could not find selected meshes with SMD raw-vertex metadata. "
            "Re-import the base SMD with the current plugin first.");
    }

    if (!foundAnyTarget)
    {
        return maya_smd::ReportError(
            "maya_smd: VTA export could not find any blendShape targets aliased as vta_frame_<time> on the selected meshes.");
    }

    return MS::kSuccess;
}

MStatus VtaExportSession::appendVertexAnimationFrames(const std::vector<VtaExportMeshBinding> &bindings)
{
    document_.hasVertexAnimation = true;

    MStatus status = appendBaseFrame(bindings);
    if (!status)
    {
        return MStatus::kFailure;
    }

    std::set<int> frameTimes;
    for (const VtaExportMeshBinding &binding : bindings)
    {
        for (const VtaExportTarget &target : binding.targets)
        {
            if (target.frameTime > 0)
            {
                frameTimes.insert(target.frameTime);
            }
        }
    }

    for (const int frameTime : frameTimes)
    {
        status = appendTargetFrame(frameTime, bindings);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    if (document_.vertexAnimationFrames.empty())
    {
        return maya_smd::ReportError("maya_smd: VTA export did not produce any vertexanimation frames.");
    }

    return MS::kSuccess;
}

MStatus VtaExportSession::appendBaseFrame(const std::vector<VtaExportMeshBinding> &bindings)
{
    simple_smd::VertexAnimationFrame baseFrame;
    baseFrame.time = 0;

    for (const VtaExportMeshBinding &binding : bindings)
    {
        AppendAllBaseSamples(binding, baseFrame);
    }

    std::sort(
        baseFrame.samples.begin(),
        baseFrame.samples.end(),
        [](const simple_smd::VertexAnimationSample &lhs, const simple_smd::VertexAnimationSample &rhs) {
            return lhs.vertexIndex < rhs.vertexIndex;
        });

    if (baseFrame.samples.empty())
    {
        return maya_smd::ReportError("maya_smd: VTA export could not build a base vertexanimation frame.");
    }

    document_.vertexAnimationFrames.push_back(baseFrame);
    return MS::kSuccess;
}

MStatus VtaExportSession::appendTargetFrame(int frameTime, const std::vector<VtaExportMeshBinding> &bindings)
{
    simple_smd::VertexAnimationFrame frame;
    frame.time = frameTime;

    for (const VtaExportMeshBinding &binding : bindings)
    {
        auto targetIt = std::find_if(
            binding.targets.begin(),
            binding.targets.end(),
            [frameTime](const VtaExportTarget &target) {
                return target.frameTime == frameTime;
            });
        if (targetIt == binding.targets.end())
        {
            continue;
        }

        MDagPath targetMeshPath;
        MString temporaryTargetTransform;
        if (!TryRegenerateBlendShapeTarget(
                targetIt->blendShapeNodeName,
                targetIt->weightIndex,
                targetMeshPath,
                temporaryTargetTransform))
        {
            return maya_smd::ReportError(
                MString("maya_smd: failed to regenerate blendShape target for VTA export frame ")
                + std::to_string(frameTime).c_str());
        }

        MStatus status;
        MFnMesh targetMeshFn(targetMeshPath, &status);
        if (!status)
        {
            if (temporaryTargetTransform.length() > 0)
            {
                maya_cmd::DeleteNodeByName(temporaryTargetTransform);
            }
            return maya_smd::ReportError("maya_smd: failed to access regenerated target mesh for VTA export.", status);
        }

        MPointArray targetPoints;
        MFloatVectorArray targetNormals;
        status = targetMeshFn.getPoints(targetPoints, MSpace::kObject);
        if (status)
        {
            status = targetMeshFn.getVertexNormals(false, targetNormals, MSpace::kObject);
        }
        if (!status)
        {
            if (temporaryTargetTransform.length() > 0)
            {
                maya_cmd::DeleteNodeByName(temporaryTargetTransform);
            }
            return maya_smd::ReportError("maya_smd: failed to read regenerated target mesh data for VTA export.", status);
        }

        AppendChangedTargetSamples(binding, targetPoints, targetNormals, frame);

        if (temporaryTargetTransform.length() > 0)
        {
            maya_cmd::DeleteNodeByName(temporaryTargetTransform);
        }
    }

    std::sort(
        frame.samples.begin(),
        frame.samples.end(),
        [](const simple_smd::VertexAnimationSample &lhs, const simple_smd::VertexAnimationSample &rhs) {
            return lhs.vertexIndex < rhs.vertexIndex;
        });

    if (!frame.samples.empty())
    {
        document_.vertexAnimationFrames.push_back(frame);
    }

    return MS::kSuccess;
}

MStatus VtaExportSession::serialize()
{
    serialized_ = document_.Serialize();
    if (serialized_.empty())
    {
        return maya_smd::ReportError(MString("maya_smd: VTA exporter produced empty output for ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}

MStatus VtaExportSession::writeOutput() const
{
    std::ofstream output(fileObject_.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return maya_smd::ReportError(MString("maya_smd: failed to open VTA output file ") + fileObject_.rawFullName());
    }

    output.write(serialized_.data(), static_cast<std::streamsize>(serialized_.size()));
    output.close();
    if (!output)
    {
        return maya_smd::ReportError(MString("maya_smd: failed to write VTA output file ") + fileObject_.rawFullName());
    }

    return MS::kSuccess;
}
