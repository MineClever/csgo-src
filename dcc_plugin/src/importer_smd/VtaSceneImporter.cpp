#include "VtaSceneImporter.h"

#include <common/MayaCommandUtils.h>
#include <common_smd/MayaSmdCommon.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>

namespace
{
constexpr const char *kSmdRawVertexMapAttribute = "mayaSmdRawVertexMap";

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
        const MObject childObject = dagNode.child(childIndex, &status);
        if (!status || !childObject.hasFn(MFn::kMesh))
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDagNode meshDagNode(childObject, &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            return childObject;
        }
        status = MS::kSuccess;
    }

    return MObject::kNullObj;
}

MString BuildFrameAliasName(int frameTime)
{
    MString alias("vta_frame_");
    alias += std::to_string(frameTime).c_str();
    return alias;
}

MString ResolveUniqueAliasName(const MObject &blendShapeObject, const MString &baseAlias)
{
    MStringArray aliasPairs;
    if (maya_cmd::GetNodeAliasList(blendShapeObject, aliasPairs) != MS::kSuccess)
    {
        return baseAlias;
    }

    auto aliasExists = [&](const MString &candidate) {
        for (unsigned int aliasIndex = 0; aliasIndex + 1 < aliasPairs.length(); aliasIndex += 2)
        {
            if (aliasPairs[aliasIndex] == candidate)
            {
                return true;
            }
        }
        return false;
    };

    if (!aliasExists(baseAlias))
    {
        return baseAlias;
    }

    for (unsigned int suffixIndex = 1; suffixIndex < 10000; ++suffixIndex)
    {
        MString candidate = baseAlias;
        candidate += "_";
        candidate += static_cast<int>(suffixIndex);
        if (!aliasExists(candidate))
        {
            return candidate;
        }
    }

    return baseAlias;
}

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

void CollectSelectedMeshBindingsRecursive(const MDagPath &candidatePath, std::vector<VtaMeshBinding> &bindings)
{
    MStatus status;
    if (candidatePath.node().hasFn(MFn::kMesh))
    {
        MFnDagNode meshDagNode(candidatePath.node(), &status);
        if (status && !meshDagNode.isIntermediateObject())
        {
            VtaMeshBinding binding;
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

        if (childObject.hasFn(MFn::kTransform) && !childObject.hasFn(MFn::kJoint))
        {
            MDagPath childPath = candidatePath;
            childPath.push(childObject);
            CollectSelectedMeshBindingsRecursive(childPath, bindings);
            continue;
        }

        if (!childObject.hasFn(MFn::kMesh))
        {
            continue;
        }

        MFnDagNode meshDagNode(childObject, &status);
        if (!status || meshDagNode.isIntermediateObject())
        {
            status = MS::kSuccess;
            continue;
        }

        VtaMeshBinding binding;
        status = MDagPath::getAPathTo(childObject, binding.meshPath);
        if (!status)
        {
            if (childObject.hasFn(MFn::kTransform) && !childObject.hasFn(MFn::kJoint))
            {
                MDagPath childPath = candidatePath;
                childPath.push(childObject);
                CollectSelectedMeshBindingsRecursive(childPath, bindings);
            }
            status = MS::kSuccess;
            continue;
        }
        binding.transformPath = candidatePath;
        bindings.push_back(binding);
    }
}
}

VtaSceneImporter::VtaSceneImporter(
    std::shared_ptr<const simple_smd::Document> document,
    const SmdImportOptions &importOptions)
    : document_(document)
    , importOptions_(importOptions)
{
}

MStatus VtaSceneImporter::Import() const
{
    if (!document_ || document_->vertexAnimationFrames.empty())
    {
        return maya_smd::ReportError("maya_smd: VTA import requires vertexanimation frames.");
    }

    std::vector<VtaMeshBinding> bindings;
    MStatus status = resolveTargetMeshes(bindings);
    if (!status)
    {
        return MStatus::kFailure;
    }

    unsigned int importedTargetCount = 0;
    for (const simple_smd::VertexAnimationFrame &frame : document_->vertexAnimationFrames)
    {
        if (frame.time <= 0)
        {
            continue;
        }

        std::unordered_set<size_t> matchedSampleIndices;
        for (VtaMeshBinding &binding : bindings)
        {
            std::vector<const simple_smd::VertexAnimationSample *> frameSamples;
            frameSamples.reserve(frame.samples.size());
            for (size_t sampleIndex = 0; sampleIndex < frame.samples.size(); ++sampleIndex)
            {
                const simple_smd::VertexAnimationSample &sample = frame.samples[sampleIndex];
                if (sample.vertexIndex < 0 ||
                    static_cast<size_t>(sample.vertexIndex) >= binding.rawToLocalVertexIndex.size())
                {
                    continue;
                }

                if (binding.rawToLocalVertexIndex[static_cast<size_t>(sample.vertexIndex)] < 0)
                {
                    continue;
                }

                frameSamples.push_back(&sample);
                matchedSampleIndices.insert(sampleIndex);
            }

            if (frameSamples.empty())
            {
                continue;
            }

            if (binding.blendShapeObject.isNull())
            {
                status = initializeBlendShapeBinding(binding);
                if (!status)
                {
                    return MStatus::kFailure;
                }
            }

            MObject targetMeshObject;
            MString targetTransformName;
            status = createFrameTarget(binding, frame, frameSamples, targetMeshObject, targetTransformName);
            if (!status)
            {
                return MStatus::kFailure;
            }

            MFnBlendShapeDeformer blendShapeFn(binding.blendShapeObject, &status);
            if (!status)
            {
                maya_cmd::DeleteNodeByName(targetTransformName);
                return maya_smd::ReportError("maya_smd: failed to bind blendShape deformer for VTA import.", status);
            }

            status = blendShapeFn.addTarget(
                binding.meshPath.node(),
                static_cast<int>(binding.nextWeightIndex),
                targetMeshObject,
                1.0);
            if (!status)
            {
                maya_cmd::DeleteNodeByName(targetTransformName);
                return maya_smd::ReportError(
                    MString("maya_smd: failed to add VTA blendShape target for frame ")
                    + std::to_string(frame.time).c_str(),
                    status);
            }

            MPlug weightElement = binding.weightArrayPlug.elementByLogicalIndex(binding.nextWeightIndex, &status);
            if (status)
            {
                maya_cmd::SetNodePlugAlias(
                    binding.blendShapeObject,
                    weightElement,
                    ResolveUniqueAliasName(binding.blendShapeObject, BuildFrameAliasName(frame.time)));
            }
            status = MS::kSuccess;

            maya_cmd::DeleteNodeByName(targetTransformName);
            ++binding.nextWeightIndex;
            ++importedTargetCount;
        }

        if (matchedSampleIndices.size() != frame.samples.size())
        {
            return maya_smd::ReportError(
                MString("maya_smd: VTA frame ")
                + std::to_string(frame.time).c_str()
                + " could not be fully mapped onto the selected meshes. "
                + "Re-import the base SMD with current importer metadata and select the full mesh set or import root.",
                MS::kFailure);
        }
    }

    if (importedTargetCount == 0)
    {
        return maya_smd::ReportWarning("maya_smd: VTA file did not contain any non-base vertexanimation targets to import.");
    }

    return MS::kSuccess;
}

MStatus VtaSceneImporter::resolveTargetMeshes(std::vector<VtaMeshBinding> &bindings) const
{
    bindings.clear();

    MSelectionList selection;
    MStatus status = MGlobal::getActiveSelectionList(selection);
    if (!status || selection.length() == 0)
    {
        return maya_smd::ReportError("maya_smd: VTA import requires selecting the target mesh, meshes, or import root.");
    }

    for (unsigned int index = 0; index < selection.length(); ++index)
    {
        MDagPath candidatePath;
        status = selection.getDagPath(index, candidatePath);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        CollectSelectedMeshBindingsRecursive(candidatePath, bindings);
    }

    std::vector<VtaMeshBinding> filteredBindings;
    filteredBindings.reserve(bindings.size());
    for (VtaMeshBinding &binding : bindings)
    {
        if (LoadRawVertexMap(binding.meshPath.node(), binding.rawToLocalVertexIndex))
        {
            filteredBindings.push_back(binding);
        }
    }

    bindings.swap(filteredBindings);
    if (bindings.empty())
    {
        return maya_smd::ReportError(
            "maya_smd: VTA import could not find selected meshes with SMD raw-vertex metadata. "
            "Re-import the base SMD with the current plugin first.");
    }

    return MS::kSuccess;
}

MStatus VtaSceneImporter::ensureBlendShapeNode(const MObject &meshObject, MObject &blendShapeObject) const
{
    blendShapeObject = MObject::kNullObj;

    MStatus status;
    MObject rootObject = meshObject;
    MItDependencyGraph iterator(
        rootObject,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (status)
    {
        for (; !iterator.isDone(); iterator.next())
        {
            MObject currentNode = iterator.currentItem(&status);
            if (status && !currentNode.isNull())
            {
                blendShapeObject = currentNode;
                return MS::kSuccess;
            }
            status = MS::kSuccess;
        }
    }

    MFnBlendShapeDeformer blendShapeFn;
    blendShapeObject = blendShapeFn.create(meshObject, MFnBlendShapeDeformer::kLocalOrigin, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to create blendShape deformer for VTA import.", status);
    }

    MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
    if (status)
    {
        blendShapeNodeFn.setName("mayaVtaBlendShape#");
    }

    return MS::kSuccess;
}

MStatus VtaSceneImporter::initializeBlendShapeBinding(VtaMeshBinding &binding) const
{
    MStatus status = ensureBlendShapeNode(binding.meshPath.node(), binding.blendShapeObject);
    if (!status)
    {
        return MStatus::kFailure;
    }

    MFnBlendShapeDeformer blendShapeFn(binding.blendShapeObject, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to bind blendShape deformer for VTA import.", status);
    }

    MIntArray weightIndices;
    status = blendShapeFn.weightIndexList(weightIndices);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to query blendShape weight indices for VTA import.", status);
    }

    binding.nextWeightIndex = 0;
    for (unsigned int index = 0; index < weightIndices.length(); ++index)
    {
        binding.nextWeightIndex = std::max(binding.nextWeightIndex, static_cast<unsigned int>(weightIndices[index] + 1));
    }

    MFnDependencyNode blendShapeNodeFn(binding.blendShapeObject, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access blendShape dependency node for VTA import.", status);
    }

    binding.weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
    if (!status)
    {
        return maya_smd::ReportError("maya_smd: failed to access blendShape weight plug for VTA import.", status);
    }

    return MS::kSuccess;
}

MStatus VtaSceneImporter::createFrameTarget(
    const VtaMeshBinding &binding,
    const simple_smd::VertexAnimationFrame &frame,
    const std::vector<const simple_smd::VertexAnimationSample *> &frameSamples,
    MObject &targetMeshObject,
    MString &targetTransformName) const
{
    targetMeshObject = MObject::kNullObj;
    targetTransformName.clear();

    MObject duplicateTransformObject;
    MDagPath duplicateTransformPath;
    MStatus status = maya_cmd::DuplicateDagNode(binding.transformPath, duplicateTransformObject, &duplicateTransformPath);
    if (!status)
    {
        return maya_smd::ReportError(
            MString("maya_smd: failed to duplicate target mesh for VTA frame ")
            + std::to_string(frame.time).c_str(),
            status);
    }

    targetTransformName = duplicateTransformPath.fullPathName();
    targetMeshObject = FindPrimaryMeshChild(duplicateTransformObject);
    if (targetMeshObject.isNull())
    {
        maya_cmd::DeleteNodeByName(targetTransformName);
        return maya_smd::ReportError(
            MString("maya_smd: duplicated VTA target did not contain a mesh for frame ")
            + std::to_string(frame.time).c_str(),
            MS::kFailure);
    }

    MFnMesh sourceMeshFn(binding.meshPath, &status);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(targetTransformName);
        return maya_smd::ReportError("maya_smd: failed to read source mesh for VTA import.", status);
    }

    MPointArray targetPoints;
    status = sourceMeshFn.getPoints(targetPoints, MSpace::kObject);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(targetTransformName);
        return maya_smd::ReportError("maya_smd: failed to read source mesh points for VTA import.", status);
    }

    for (const simple_smd::VertexAnimationSample *sample : frameSamples)
    {
        const int localVertexIndex = binding.rawToLocalVertexIndex[static_cast<size_t>(sample->vertexIndex)];
        targetPoints.set(
            MPoint(sample->px, sample->py, sample->pz),
            static_cast<unsigned int>(localVertexIndex));
    }

    MFnMesh targetMeshFn(targetMeshObject, &status);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(targetTransformName);
        return maya_smd::ReportError("maya_smd: failed to access duplicated target mesh for VTA import.", status);
    }

    status = targetMeshFn.setPoints(targetPoints, MSpace::kObject);
    if (!status)
    {
        maya_cmd::DeleteNodeByName(targetTransformName);
        return maya_smd::ReportError(
            MString("maya_smd: failed to apply VTA target points for frame ")
            + std::to_string(frame.time).c_str(),
            status);
    }

    return MS::kSuccess;
}
