#pragma once

#include <maya/MNamespace.h>
#include <maya/MString.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace dcc_import_policy
{

enum class RootMode
{
    DedicatedRoot,
    SceneRoot,
};

enum class ObjectMergeMode
{
    CreateNew,
    UpdateScene,
    AppendMissing,
    AnimationOnly,
};

enum class AnimationImportMode
{
    None,
    NewLayer,
    ReplaceLayer,
};

struct SceneImportPolicy
{
    RootMode rootMode = RootMode::DedicatedRoot;
    ObjectMergeMode objectMergeMode = ObjectMergeMode::CreateNew;
    bool readNamespaceFromScene = true;
    std::string currentNamespace;
    bool importAnimationToLayer = false;
    AnimationImportMode animationImportMode = AnimationImportMode::None;
    std::string animationLayerName;
};

inline std::unordered_map<std::string, std::string> ParseOptionMap(const MString &options)
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

inline bool ParseBoolOption(
    const std::unordered_map<std::string, std::string> &optionMap,
    const char *key,
    bool defaultValue)
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

inline SceneImportPolicy ParseSceneImportPolicy(const std::unordered_map<std::string, std::string> &optionMap)
{
    SceneImportPolicy policy;
    const bool useSceneRoot =
        ParseBoolOption(optionMap, "usesceneroot", false) ||
        ParseBoolOption(optionMap, "usesceneasroot", false);
    policy.rootMode = useSceneRoot ? RootMode::SceneRoot : RootMode::DedicatedRoot;
    policy.readNamespaceFromScene = ParseBoolOption(optionMap, "readnamespacefromscene", true);

    auto modeIt = optionMap.find("importmode");
    if (modeIt != optionMap.end())
    {
        std::string importMode = modeIt->second;
        std::transform(importMode.begin(), importMode.end(), importMode.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (importMode == "update")
        {
            policy.objectMergeMode = ObjectMergeMode::UpdateScene;
        }
        else if (importMode == "append")
        {
            policy.objectMergeMode = ObjectMergeMode::AppendMissing;
        }
        else if (importMode == "animationonly")
        {
            policy.objectMergeMode = ObjectMergeMode::AnimationOnly;
        }
    }

    policy.importAnimationToLayer =
        ParseBoolOption(optionMap, "importanimationtolayer", false) ||
        ParseBoolOption(optionMap, "useanimationlayer", false);

    auto animationLayerModeIt = optionMap.find("animationlayermode");
    if (policy.importAnimationToLayer || animationLayerModeIt != optionMap.end())
    {
        policy.animationImportMode = AnimationImportMode::NewLayer;
        if (animationLayerModeIt != optionMap.end())
        {
            std::string animationLayerMode = animationLayerModeIt->second;
            std::transform(animationLayerMode.begin(), animationLayerMode.end(), animationLayerMode.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (animationLayerMode == "replace")
            {
                policy.animationImportMode = AnimationImportMode::ReplaceLayer;
            }
        }
    }

    auto animationLayerNameIt = optionMap.find("animationlayername");
    if (animationLayerNameIt != optionMap.end())
    {
        policy.animationLayerName = animationLayerNameIt->second;
    }

    return policy;
}

inline void CaptureCurrentNamespace(SceneImportPolicy &policy)
{
    if (!policy.readNamespaceFromScene)
    {
        policy.currentNamespace.clear();
        return;
    }

    MStatus status;
    const MString currentNamespace = MNamespace::currentNamespace(&status);
    if (!status || currentNamespace.length() == 0 || currentNamespace == ":")
    {
        policy.currentNamespace.clear();
        return;
    }

    policy.currentNamespace = currentNamespace.asChar();
}

inline std::string StripNamespace(std::string nodeName)
{
    const size_t lastNamespaceSeparator = nodeName.rfind(':');
    if (lastNamespaceSeparator == std::string::npos)
    {
        return nodeName;
    }

    return nodeName.substr(lastNamespaceSeparator + 1);
}

inline bool MatchesImportedRenameSuffix(const std::string &sceneNodeName, const std::string &targetNodeName)
{
    if (sceneNodeName.size() <= targetNodeName.size())
    {
        return false;
    }

    if (sceneNodeName.compare(sceneNodeName.size() - targetNodeName.size(), targetNodeName.size(), targetNodeName) != 0)
    {
        return false;
    }

    const size_t separatorIndex = sceneNodeName.size() - targetNodeName.size() - 1;
    const char separator = sceneNodeName[separatorIndex];
    return separator == '_';
}

inline std::string TrimTrailingDigits(std::string nodeName)
{
    while (!nodeName.empty() && std::isdigit(static_cast<unsigned char>(nodeName.back())))
    {
        nodeName.pop_back();
    }

    return nodeName;
}

inline std::string ComposeNamespacedName(const SceneImportPolicy &policy, const std::string &nodeName)
{
    if (policy.currentNamespace.empty())
    {
        return nodeName;
    }

    return policy.currentNamespace + ":" + nodeName;
}

inline bool MatchesNodeNameForAppend(
    const SceneImportPolicy &policy,
    const std::string &sceneNodeName,
    const std::string &targetNodeName)
{
    if (sceneNodeName == targetNodeName)
    {
        return true;
    }

    if (StripNamespace(sceneNodeName) == targetNodeName)
    {
        return true;
    }

    const std::string namespacedTargetName = ComposeNamespacedName(policy, targetNodeName);
    if (sceneNodeName == namespacedTargetName)
    {
        return true;
    }

    const std::string strippedSceneNodeName = StripNamespace(sceneNodeName);
    const std::string strippedTargetName = StripNamespace(namespacedTargetName);
    if (strippedSceneNodeName == strippedTargetName)
    {
        return true;
    }

    return MatchesImportedRenameSuffix(strippedSceneNodeName, strippedTargetName) ||
        MatchesImportedRenameSuffix(strippedTargetName, strippedSceneNodeName);
}

inline bool MatchesNodePrefixForAppend(
    const SceneImportPolicy &policy,
    const std::string &sceneNodeName,
    const std::string &targetNodePrefix)
{
    const std::string strippedSceneNodeName = TrimTrailingDigits(StripNamespace(sceneNodeName));
    if (strippedSceneNodeName.rfind(targetNodePrefix, 0) == 0)
    {
        return true;
    }

    const std::string namespacedTargetName = ComposeNamespacedName(policy, targetNodePrefix);
    const std::string strippedTargetNodePrefix = TrimTrailingDigits(StripNamespace(namespacedTargetName));
    if (sceneNodeName.rfind(namespacedTargetName, 0) == 0)
    {
        return true;
    }

    return MatchesImportedRenameSuffix(strippedSceneNodeName, strippedTargetNodePrefix) ||
        MatchesImportedRenameSuffix(strippedTargetNodePrefix, strippedSceneNodeName);
}

inline bool UsesDedicatedRoot(const SceneImportPolicy &policy)
{
    return policy.rootMode == RootMode::DedicatedRoot;
}

inline bool UsesSceneRoot(const SceneImportPolicy &policy)
{
    return policy.rootMode == RootMode::SceneRoot;
}

inline bool UsesUpdateCurrentScene(const SceneImportPolicy &policy)
{
    return policy.objectMergeMode == ObjectMergeMode::UpdateScene;
}

inline bool UsesAppendMissingObjects(const SceneImportPolicy &policy)
{
    return policy.objectMergeMode == ObjectMergeMode::AppendMissing;
}

inline bool UsesExistingObjectMerge(const SceneImportPolicy &policy)
{
    return policy.objectMergeMode == ObjectMergeMode::AppendMissing ||
        policy.objectMergeMode == ObjectMergeMode::UpdateScene ||
        policy.objectMergeMode == ObjectMergeMode::AnimationOnly;
}

inline bool UsesAnimationOnlyImport(const SceneImportPolicy &policy)
{
    return policy.objectMergeMode == ObjectMergeMode::AnimationOnly;
}

inline bool UsesAnimationLayerImport(const SceneImportPolicy &policy)
{
    return policy.importAnimationToLayer;
}

} // namespace dcc_import_policy
