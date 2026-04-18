#pragma once

#include <common/ImportPolicy.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dcc_import_policy
{

class SceneMergeStrategy
{
public:
    SceneMergeStrategy() = default;

    explicit SceneMergeStrategy(SceneImportPolicy policy)
        : policy_(std::move(policy))
    {
    }

    static SceneMergeStrategy Parse(const std::unordered_map<std::string, std::string> &optionMap)
    {
        return SceneMergeStrategy(ParseSceneImportPolicy(optionMap));
    }

    const SceneImportPolicy &policy() const
    {
        return policy_;
    }

    SceneImportPolicy &policy()
    {
        return policy_;
    }

    void captureCurrentNamespace()
    {
        CaptureCurrentNamespace(policy_);
    }

    void normalizeForImport(const std::string &sourceName)
    {
        if (UsesAnimationOnlyImport(policy_) && !UsesSceneRoot(policy_))
        {
            policy_.rootMode = RootMode::SceneRoot;
        }

        if (UsesSourceDeltaImport(policy_))
        {
            policy_.importAnimationToLayer = true;
            if (policy_.animationImportMode == AnimationImportMode::None)
            {
                policy_.animationImportMode = AnimationImportMode::NewLayer;
            }
        }

        if (policy_.importAnimationToLayer && policy_.animationLayerName.empty())
        {
            policy_.animationLayerName = ResolveDefaultAnimationLayerName(sourceName);
        }
    }

    bool usesDedicatedRoot() const
    {
        return UsesDedicatedRoot(policy_);
    }

    bool usesSceneRoot() const
    {
        return UsesSceneRoot(policy_);
    }

    bool usesUpdateCurrentScene() const
    {
        return UsesUpdateCurrentScene(policy_);
    }

    bool usesAppendMissingObjects() const
    {
        return UsesAppendMissingObjects(policy_);
    }

    bool usesExistingObjectMerge() const
    {
        return UsesExistingObjectMerge(policy_);
    }

    bool usesAnimationOnlyImport() const
    {
        return UsesAnimationOnlyImport(policy_);
    }

    bool usesAnimationLayerImport() const
    {
        return UsesAnimationLayerImport(policy_);
    }

    bool usesSourceDeltaImport() const
    {
        return UsesSourceDeltaImport(policy_);
    }

    bool shouldApplyBaseTransformToNode(bool reusedExistingNode) const
    {
        return !reusedExistingNode ||
            (UsesUpdateCurrentScene(policy_) &&
             !UsesAnimationLayerImport(policy_) &&
             !UsesAnimationOnlyImport(policy_));
    }

    bool shouldImportShapes() const
    {
        return !UsesAnimationOnlyImport(policy_);
    }

    bool matchesNodeNameForAppend(
        const std::string &sceneNodeName,
        const std::string &targetNodeName) const
    {
        return MatchesNodeNameForAppend(policy_, sceneNodeName, targetNodeName);
    }

    bool matchesNodePrefixForAppend(
        const std::string &sceneNodeName,
        const std::string &targetNodePrefix) const
    {
        return MatchesNodePrefixForAppend(policy_, sceneNodeName, targetNodePrefix);
    }

    std::string resolveDefaultAnimationLayerName(const std::string &sourceName) const
    {
        return ResolveDefaultAnimationLayerName(sourceName);
    }

private:
    static std::string SanitizeLayerName(std::string value)
    {
        for (char &character : value)
        {
            if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
            {
                character = '_';
            }
        }

        return value.empty() ? std::string("layer") : value;
    }

    std::string ResolveDefaultAnimationLayerName(const std::string &sourceName) const
    {
        return SanitizeLayerName(sourceName) +
            (UsesSourceDeltaImport(policy_) ? "_source_delta" : "_layer");
    }

    SceneImportPolicy policy_;
};

class SceneMergeResolver
{
public:
    SceneMergeResolver() = default;

    explicit SceneMergeResolver(SceneImportPolicy policy)
        : strategy_(std::move(policy))
    {
    }

    explicit SceneMergeResolver(SceneMergeStrategy strategy)
        : strategy_(std::move(strategy))
    {
    }

    static SceneMergeResolver Parse(const std::unordered_map<std::string, std::string> &optionMap)
    {
        return SceneMergeResolver(SceneMergeStrategy::Parse(optionMap));
    }

    SceneMergeStrategy &strategy()
    {
        return strategy_;
    }

    const SceneMergeStrategy &strategy() const
    {
        return strategy_;
    }

    SceneImportPolicy &policy()
    {
        return strategy_.policy();
    }

    const SceneImportPolicy &policy() const
    {
        return strategy_.policy();
    }

    void captureCurrentNamespace()
    {
        strategy_.captureCurrentNamespace();
    }

    void normalizeForImport(const std::string &sourceName)
    {
        strategy_.normalizeForImport(sourceName);
    }

    bool usesDedicatedRoot() const
    {
        return strategy_.usesDedicatedRoot();
    }

    bool usesSceneRoot() const
    {
        return strategy_.usesSceneRoot();
    }

    bool usesUpdateCurrentScene() const
    {
        return strategy_.usesUpdateCurrentScene();
    }

    bool usesAppendMissingObjects() const
    {
        return strategy_.usesAppendMissingObjects();
    }

    bool usesExistingObjectMerge() const
    {
        return strategy_.usesExistingObjectMerge();
    }

    bool usesAnimationOnlyImport() const
    {
        return strategy_.usesAnimationOnlyImport();
    }

    bool usesAnimationLayerImport() const
    {
        return strategy_.usesAnimationLayerImport();
    }

    bool usesSourceDeltaImport() const
    {
        return strategy_.usesSourceDeltaImport();
    }

    bool shouldApplyBaseTransformToNode(bool reusedExistingNode) const
    {
        return strategy_.shouldApplyBaseTransformToNode(reusedExistingNode);
    }

    bool shouldImportShapes() const
    {
        return strategy_.shouldImportShapes();
    }

    bool matchesNodeNameForAppend(
        const std::string &sceneNodeName,
        const std::string &targetNodeName) const
    {
        return strategy_.matchesNodeNameForAppend(sceneNodeName, targetNodeName);
    }

    bool matchesNodePrefixForAppend(
        const std::string &sceneNodeName,
        const std::string &targetNodePrefix) const
    {
        return strategy_.matchesNodePrefixForAppend(sceneNodeName, targetNodePrefix);
    }

    std::string resolveDefaultAnimationLayerName(const std::string &sourceName) const
    {
        return strategy_.resolveDefaultAnimationLayerName(sourceName);
    }

    MObject findAppendTargetChild(
        const MObject &parent,
        const std::string &nodeName,
        bool requireJoint) const
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
                if (dagIterator.getPath(dagPath) != MS::kSuccess)
                {
                    continue;
                }

                MFnDagNode dagNode(dagPath, &status);
                if (!status || !strategy_.matchesNodeNameForAppend(dagNode.name().asChar(), nodeName))
                {
                    continue;
                }

                if (requireJoint)
                {
                    if (dagPath.hasFn(MFn::kJoint))
                    {
                        return dagPath.node();
                    }
                    continue;
                }

                if (dagPath.hasFn(MFn::kTransform) || dagPath.hasFn(MFn::kJoint))
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
            if (!status)
            {
                status = MS::kSuccess;
                continue;
            }

            if (!(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
            {
                continue;
            }

            MFnDagNode childDagNode(childObject, &status);
            if (!status || !strategy_.matchesNodeNameForAppend(childDagNode.name().asChar(), nodeName))
            {
                status = MS::kSuccess;
                continue;
            }

            if (requireJoint)
            {
                if (childObject.hasFn(MFn::kJoint))
                {
                    return childObject;
                }
                continue;
            }

            return childObject;
        }

        return MObject::kNullObj;
    }

private:
    SceneMergeStrategy strategy_;
};

} // namespace dcc_import_policy
