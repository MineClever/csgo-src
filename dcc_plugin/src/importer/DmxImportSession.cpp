#include "DmxImportSession.h"

#include "DmxImportAnimation.h"
#include "DmxImportDag.h"
#include "DmxImportInternals.h"
#include "DmxImportMeshMaterial.h"

#include <common/ImportTransformCorrection.h>
#include <common/SimpleDmxDocument.h>
#include <common/MayaDmxCommon.h>
#include <common/SceneMergeStrategy.h>
#include <common/SimpleDmxText.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MEulerRotation.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>
using simple_dmx::FindAttributeElementArray;
using simple_dmx::FindAttributeString;
using dmx_import_translator::ImportContext;
using namespace dmx_import_impl;

namespace
{
struct DmxImportDocumentNormalizer
{
    static MMatrix ComputeLegacyAxisCorrectionMatrix(const std::string &sourceUpAxis, MString &warning)
    {
        const std::string normalizedSourceUpAxis = NormalizeAxisName(sourceUpAxis);
        if (normalizedSourceUpAxis.empty())
        {
            return MMatrix::identity;
        }

        MStatus status;
        const bool mayaYAxisUp = MGlobal::isYAxisUp(&status);
        if (!status)
        {
            return MMatrix::identity;
        }

        const bool mayaZAxisUp = MGlobal::isZAxisUp(&status);
        if (!status)
        {
            return MMatrix::identity;
        }

        if (normalizedSourceUpAxis == "Z" && mayaYAxisUp)
        {
            dcc_import_transform::TransformCorrection correction;
            correction.rotation.x = -1.57079632679;
            warning = "maya_dmx: applied legacy Z-up to Y-up import correction.";
            return correction.Matrix();
        }

        if (normalizedSourceUpAxis == "Y" && mayaZAxisUp)
        {
            dcc_import_transform::TransformCorrection correction;
            correction.rotation.x = 1.57079632679;
            warning = "maya_dmx: applied legacy Y-up to Z-up import correction.";
            return correction.Matrix();
        }

        return MMatrix::identity;
    }

    static std::string FormatVector3(const MVector &value)
    {
        std::ostringstream stream;
        stream << std::setprecision(17) << value.x << ' ' << value.y << ' ' << value.z;
        return stream.str();
    }

    static std::string FormatQuaternion(const MQuaternion &value)
    {
        std::ostringstream stream;
        stream << std::setprecision(17) << value.x << ' ' << value.y << ' ' << value.z << ' ' << value.w;
        return stream.str();
    }

    static void SetScalarStringAttribute(simple_dmx::Element &element, const char *attributeName, const std::string &fallbackType, const std::string &value)
    {
        std::string declaredType = fallbackType;
        auto attributeIt = element.attributes.find(attributeName);
        if (attributeIt != element.attributes.end() && !attributeIt->second.declaredType.empty())
        {
            declaredType = attributeIt->second.declaredType;
        }

        simple_dmx::SetAttr(element, attributeName, simple_dmx::ScalarAttr(declaredType, value));
    }

    static void SetStringArrayAttribute(
        simple_dmx::Element &element,
        const char *attributeName,
        const std::string &fallbackType,
        std::vector<std::string> values)
    {
        std::string declaredType = fallbackType;
        auto attributeIt = element.attributes.find(attributeName);
        if (attributeIt != element.attributes.end() && !attributeIt->second.declaredType.empty())
        {
            declaredType = attributeIt->second.declaredType;
        }

        simple_dmx::SetAttr(element, attributeName, simple_dmx::ScalarArrayAttr(declaredType, std::move(values)));
    }
};

void ApplyCorrectionToTransformElement(
    simple_dmx::Element &transformElement,
    const dcc_import_transform::TransformCorrection &correction,
    bool isTopLevelTransform)
{
    const std::vector<double> positionValues = simple_dmx::ParseNumberList(
        FindAttributeString(&transformElement, "position"));
    const std::vector<double> orientationValues = simple_dmx::ParseNumberList(
        FindAttributeString(&transformElement, "orientation"));

    MVector correctedPosition(0.0, 0.0, 0.0);
    if (positionValues.size() >= 3)
    {
        correctedPosition = dcc_import_transform::ApplyToTranslationScale(
            correction,
            MVector(positionValues[0], positionValues[1], positionValues[2]));
        if (isTopLevelTransform)
        {
            correctedPosition = dcc_import_transform::ApplyToTopLevelTranslation(
                correction,
                MVector(positionValues[0], positionValues[1], positionValues[2]));
        }
        DmxImportDocumentNormalizer::SetScalarStringAttribute(transformElement, "position", "vector3", DmxImportDocumentNormalizer::FormatVector3(correctedPosition));
    }
    else if (isTopLevelTransform && !correction.IsIdentity())
    {
        correctedPosition = dcc_import_transform::ApplyToTopLevelTranslation(correction, MVector::zero);
        DmxImportDocumentNormalizer::SetScalarStringAttribute(transformElement, "position", "vector3", DmxImportDocumentNormalizer::FormatVector3(correctedPosition));
    }

    if (!isTopLevelTransform)
    {
        return;
    }

    MQuaternion correctedOrientation;
    if (orientationValues.size() >= 4)
    {
        correctedOrientation = dcc_import_transform::ApplyToQuaternion(
            correction,
            MQuaternion(
                orientationValues[0],
                orientationValues[1],
                orientationValues[2],
                orientationValues[3]));
    }
    else
    {
        correctedOrientation = dcc_import_transform::ApplyToQuaternion(
            correction,
            MEulerRotation().asQuaternion());
    }
    DmxImportDocumentNormalizer::SetScalarStringAttribute(transformElement, "orientation", "quaternion", DmxImportDocumentNormalizer::FormatQuaternion(correctedOrientation));
}

void ApplyCorrectionToMeshVertexData(
    simple_dmx::Element &vertexData,
    const dcc_import_transform::TransformCorrection &scaleCorrection)
{
    const std::vector<std::string> positionStrings = simple_dmx::FindAttributeStringArray(&vertexData, "positions");
    if (!positionStrings.empty())
    {
        std::vector<std::string> correctedPositions;
        correctedPositions.reserve(positionStrings.size());
        for (const std::string &positionString : positionStrings)
        {
            const std::vector<double> values = simple_dmx::ParseNumberList(positionString);
            if (values.size() < 3)
            {
                correctedPositions.push_back(positionString);
                continue;
            }

            correctedPositions.push_back(DmxImportDocumentNormalizer::FormatVector3(
                dcc_import_transform::ApplyToTranslationScale(
                    scaleCorrection,
                    MVector(values[0], values[1], values[2]))));
        }
        DmxImportDocumentNormalizer::SetStringArrayAttribute(vertexData, "positions", "vector3_array", std::move(correctedPositions));
    }

    const std::vector<std::string> normalStrings = simple_dmx::FindAttributeStringArray(&vertexData, "normals");
    if (!normalStrings.empty())
    {
        std::vector<std::string> correctedNormals;
        correctedNormals.reserve(normalStrings.size());
        for (const std::string &normalString : normalStrings)
        {
            const std::vector<double> values = simple_dmx::ParseNumberList(normalString);
            if (values.size() < 3)
            {
                correctedNormals.push_back(normalString);
                continue;
            }

            correctedNormals.push_back(DmxImportDocumentNormalizer::FormatVector3(
                dcc_import_transform::ApplyToNormal(
                    scaleCorrection,
                    MVector(values[0], values[1], values[2]))));
        }
        DmxImportDocumentNormalizer::SetStringArrayAttribute(vertexData, "normals", "vector3_array", std::move(correctedNormals));
    }
}

void ApplyCorrectionToDeltaState(
    simple_dmx::Element &deltaState,
    const dcc_import_transform::TransformCorrection &scaleCorrection)
{
    const std::vector<std::string> deltaPositionStrings = simple_dmx::FindAttributeStringArray(&deltaState, "positions");
    if (deltaPositionStrings.empty())
    {
        return;
    }

    std::vector<std::string> correctedDeltaPositions;
    correctedDeltaPositions.reserve(deltaPositionStrings.size());
    for (const std::string &deltaPositionString : deltaPositionStrings)
    {
        const std::vector<double> values = simple_dmx::ParseNumberList(deltaPositionString);
        if (values.size() < 3)
        {
            correctedDeltaPositions.push_back(deltaPositionString);
            continue;
        }

        correctedDeltaPositions.push_back(DmxImportDocumentNormalizer::FormatVector3(
            dcc_import_transform::ApplyToTranslationScale(
                scaleCorrection,
                MVector(values[0], values[1], values[2]))));
    }

    DmxImportDocumentNormalizer::SetStringArrayAttribute(deltaState, "positions", "vector3_array", std::move(correctedDeltaPositions));
}

void NormalizeDagHierarchyForImportCorrection(
    simple_dmx::Document &document,
    const simple_dmx::Element *dagElement,
    const dcc_import_transform::TransformCorrection &correction,
    const dcc_import_transform::TransformCorrection &scaleCorrection,
    bool isTopLevelDag,
    std::unordered_set<std::string> &topLevelTransformKeys)
{
    if (!dagElement)
    {
        return;
    }

    if (simple_dmx::Element *transformElement = const_cast<simple_dmx::Element *>(
            simple_dmx::FindAttributeElement(document, dagElement, "transform")))
    {
        ApplyCorrectionToTransformElement(*transformElement, correction, isTopLevelDag);
        if (isTopLevelDag)
        {
            topLevelTransformKeys.insert(ElementKey(transformElement));
        }
    }

    if (simple_dmx::Element *meshElement = const_cast<simple_dmx::Element *>(
            simple_dmx::FindAttributeElement(document, dagElement, "shape")))
    {
        if (meshElement->type == "DmeMesh")
        {
            if (simple_dmx::Element *vertexData = const_cast<simple_dmx::Element *>(FindMeshVertexData(document, meshElement)))
            {
                ApplyCorrectionToMeshVertexData(*vertexData, scaleCorrection);
            }

            for (const simple_dmx::Element *deltaState : simple_dmx::FindAttributeElementArray(document, meshElement, "deltaStates"))
            {
                if (deltaState)
                {
                    ApplyCorrectionToDeltaState(*const_cast<simple_dmx::Element *>(deltaState), scaleCorrection);
                }
            }
        }
    }

    for (const simple_dmx::Element *child : simple_dmx::FindAttributeElementArray(document, dagElement, "children"))
    {
        NormalizeDagHierarchyForImportCorrection(
            document,
            child,
            correction,
            scaleCorrection,
            false,
            topLevelTransformKeys);
    }
}

void NormalizeAnimationForImportCorrection(
    simple_dmx::Document &document,
    const simple_dmx::Element *animationList,
    const std::unordered_set<std::string> &topLevelTransformKeys,
    const dcc_import_transform::TransformCorrection &correction)
{
    for (const simple_dmx::Element *animation : simple_dmx::FindAttributeElementArray(document, animationList, "animations"))
    {
        if (!animation)
        {
            continue;
        }

        for (const simple_dmx::Element *channel : simple_dmx::FindAttributeElementArray(document, animation, "channels"))
        {
            if (!channel)
            {
                continue;
            }

            const std::string targetAttribute = FindAttributeString(channel, "toAttribute");
            const simple_dmx::Element *targetElement = simple_dmx::FindAttributeElement(document, channel, "toElement");
            const bool isTopLevelTransform =
                targetElement &&
                topLevelTransformKeys.find(ElementKey(targetElement)) != topLevelTransformKeys.end();

            simple_dmx::Element *logElement = const_cast<simple_dmx::Element *>(
                simple_dmx::FindAttributeElement(document, channel, "log"));
            if (!logElement)
            {
                continue;
            }

            const std::vector<const simple_dmx::Element *> layers =
                simple_dmx::FindAttributeElementArray(document, logElement, "layers");
            if (layers.empty() || !layers.front())
            {
                continue;
            }

            simple_dmx::Element *logLayer = const_cast<simple_dmx::Element *>(layers.front());
            const std::vector<std::string> valueStrings = simple_dmx::FindAttributeStringArray(logLayer, "values");
            if (valueStrings.empty())
            {
                continue;
            }

            std::vector<std::string> correctedValues;
            correctedValues.reserve(valueStrings.size());
            if (targetAttribute == "position")
            {
                for (const std::string &valueString : valueStrings)
                {
                    const std::vector<double> values = simple_dmx::ParseNumberList(valueString);
                    if (values.size() < 3)
                    {
                        correctedValues.push_back(valueString);
                        continue;
                    }

                    MVector correctedPosition = dcc_import_transform::ApplyToTranslationScale(
                        correction,
                        MVector(values[0], values[1], values[2]));
                    if (isTopLevelTransform)
                    {
                        correctedPosition = dcc_import_transform::ApplyToTopLevelTranslation(
                            correction,
                            MVector(values[0], values[1], values[2]));
                    }
                    correctedValues.push_back(DmxImportDocumentNormalizer::FormatVector3(correctedPosition));
                }
                DmxImportDocumentNormalizer::SetStringArrayAttribute(*logLayer, "values", "vector3_array", std::move(correctedValues));
            }
            else if (targetAttribute == "orientation" && isTopLevelTransform)
            {
                for (const std::string &valueString : valueStrings)
                {
                    const std::vector<double> values = simple_dmx::ParseNumberList(valueString);
                    if (values.size() < 4)
                    {
                        correctedValues.push_back(valueString);
                        continue;
                    }

                    correctedValues.push_back(DmxImportDocumentNormalizer::FormatQuaternion(
                        dcc_import_transform::ApplyToQuaternion(
                            correction,
                            MQuaternion(values[0], values[1], values[2], values[3]))));
                }
                DmxImportDocumentNormalizer::SetStringArrayAttribute(*logLayer, "values", "quaternion_array", std::move(correctedValues));
            }
        }
    }
}

void NormalizeDocumentForImportCorrection(
    simple_dmx::Document &document,
    const simple_dmx::Element *importRoot,
    const dcc_import_transform::TransformCorrection &correction)
{
    if (!importRoot || correction.IsIdentity())
    {
        return;
    }

    dcc_import_transform::TransformCorrection scaleCorrection;
    scaleCorrection.scale[0] = correction.scale[0];
    scaleCorrection.scale[1] = correction.scale[1];
    scaleCorrection.scale[2] = correction.scale[2];

    std::unordered_set<std::string> topLevelTransformKeys;
    for (const simple_dmx::Element *child : simple_dmx::FindAttributeElementArray(document, importRoot, "children"))
    {
        NormalizeDagHierarchyForImportCorrection(
            document,
            child,
            correction,
            scaleCorrection,
            true,
            topLevelTransformKeys);
    }

    const simple_dmx::Element *documentRoot = document.GetRoot();
    const simple_dmx::Element *modelRoot = importRoot->type == "DmeModel" ? importRoot : nullptr;
    ImportContext proxyContext{document};
    auto proxyContextPtr = std::shared_ptr<ImportContext>(&proxyContext, [](ImportContext *) {});
    AnimationImporter proxyAnimator(proxyContextPtr);
    proxyAnimator.setLookupRoots(documentRoot, importRoot, modelRoot);
    const simple_dmx::Element *animationList = proxyAnimator.FindAnimationList();
    if (animationList)
    {
        NormalizeAnimationForImportCorrection(document, animationList, topLevelTransformKeys, correction);
    }
}
}

DmxImportSession::DmxImportSession(const MFileObject &fileObject, const MString &options)
    : filePath_(fileObject.rawFullName())
    , optionsText_(options)
{
}

MStatus DmxImportSession::Run()
{
    AppendImportDebugLog("session: run begin");
    MStatus status = LoadDocument();
    if (!status)
    {
        AppendImportDebugLog("session: load document failed");
        return MStatus::kFailure;
    }
    AppendImportDebugLog("session: load document ok");

    auto context = std::shared_ptr<ImportContext>(new ImportContext{document_});
    context->modelRoot = importRoot_->type == "DmeModel" ? importRoot_ : nullptr;
    context->scenePolicy = importOptions_.scenePolicy;
    context->importSkin = importOptions_.importSkin;
    context->importMaterials = importOptions_.importMaterials;
    context->importDeltaStates = importOptions_.importDeltaStates;
    if (context->modelRoot)
    {
        CollectJointInfo(document_, context->modelRoot, *context);
    }
    AppendImportDebugLog((std::string("session: context modelRoot=") + (context->modelRoot ? context->modelRoot->name : "<null>")).c_str());

    MObject sceneRoot;
    status = CreateSceneRoot(*context, sceneRoot);
    if (!status)
    {
        AppendImportDebugLog("session: create scene root failed");
        return MStatus::kFailure;
    }
    AppendImportDebugLog("session: create scene root ok");

    status = ImportHierarchy(*context, sceneRoot);
    if (!status)
    {
        AppendImportDebugLog("session: import hierarchy failed");
        return MStatus::kFailure;
    }
    AppendImportDebugLog("session: import hierarchy ok");

    status = ImportAnimation(context, sceneRoot);
    if (!status)
    {
        AppendImportDebugLog("session: import animation failed");
        return MStatus::kFailure;
    }
    AppendImportDebugLog("session: import animation ok");

    AppendImportDebugLog("session: run end success");
    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + filePath_);
}

MStatus DmxImportSession::LoadDocument()
{
    AppendImportDebugLog("session: load document begin");
    const std::string fileText = ReadTextFile(filePath_);
    if (fileText.empty())
    {
        AppendImportDebugLog("session: read text file returned empty");
        return maya_dmx::ReportError(MString("maya_dmx: failed to read file ") + filePath_);
    }
    AppendImportDebugLog((std::string("session: read text bytes=") + std::to_string(fileText.size())).c_str());

    std::string parseError;
    if (!simple_dmx::ParseDocument(fileText, document_, parseError))
    {
        AppendImportDebugLog((std::string("session: parse failed error=") + parseError).c_str());
        return maya_dmx::ReportError(MString("maya_dmx: parse error: ") + parseError.c_str());
    }
    AppendImportDebugLog("session: parse document ok");

    importRoot_ = FindImportRoot(document_);
    if (!importRoot_)
    {
        AppendImportDebugLog("session: import root missing");
        return maya_dmx::ReportError("maya_dmx: no importable DMX root element found.");
    }
    AppendImportDebugLog((std::string("session: import root type=") + importRoot_->type + " name=" + importRoot_->name).c_str());

    importOptions_ = ParseImportOptions(optionsText_);
    dcc_import_policy::SceneMergeStrategy sceneMergeStrategy(importOptions_.scenePolicy);
    sceneMergeStrategy.captureCurrentNamespace();
    const std::string resolvedPath = filePath_.asChar();
    const size_t lastSeparator = resolvedPath.find_last_of("/\\");
    const std::string baseName = lastSeparator == std::string::npos ? resolvedPath : resolvedPath.substr(lastSeparator + 1);
    sceneMergeStrategy.normalizeForImport(baseName);
    importOptions_.scenePolicy = sceneMergeStrategy.policy();
    if (sceneMergeStrategy.usesAnimationOnlyImport() && !sceneMergeStrategy.usesSceneRoot())
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=animationOnly forces useSceneRoot=1 so imported animation can target existing scene nodes.");
    }
    {
        std::ostringstream optionsSummary;
        optionsSummary
            << "session: options useSceneRoot=" << (sceneMergeStrategy.usesSceneRoot() ? "1" : "0")
            << " update=" << (sceneMergeStrategy.usesUpdateCurrentScene() ? "1" : "0")
            << " append=" << (sceneMergeStrategy.usesAppendMissingObjects() ? "1" : "0")
            << " animationOnly=" << (sceneMergeStrategy.usesAnimationOnlyImport() ? "1" : "0")
            << " importSkin=" << (importOptions_.importSkin ? "1" : "0")
            << " importMaterials=" << (importOptions_.importMaterials ? "1" : "0")
            << " importDeltaStates=" << (importOptions_.importDeltaStates ? "1" : "0")
            << " importAnimationToLayer=" << (importOptions_.scenePolicy.importAnimationToLayer ? "1" : "0")
            << " animationLayerName=" << (importOptions_.scenePolicy.animationLayerName.empty() ? "<empty>" : importOptions_.scenePolicy.animationLayerName)
            << " sourceDelta=" << static_cast<int>(importOptions_.scenePolicy.sourceDeltaMode)
            << " sourceDeltaUseClip=" << (importOptions_.scenePolicy.sourceDeltaUseClip ? "1" : "0")
            << " sourceDeltaClip=" << (importOptions_.scenePolicy.sourceDeltaClip.empty() ? "<empty>" : importOptions_.scenePolicy.sourceDeltaClip)
            << " sourceDeltaReferenceFrame=" << importOptions_.scenePolicy.sourceDeltaReferenceFrame;
        AppendImportDebugLog(optionsSummary.str().c_str());
    }

    dcc_import_transform::TransformCorrection documentCorrection = importOptions_.transformCorrection;
    if (importOptions_.applyLegacyAxisCorrection)
    {
        MString axisWarning;
        const MMatrix legacyAxisMatrix = DmxImportDocumentNormalizer::ComputeLegacyAxisCorrectionMatrix(FindAttributeString(importRoot_, "upAxis"), axisWarning);
        if (!legacyAxisMatrix.isEquivalent(MMatrix::identity))
        {
            MTransformationMatrix legacyAxisTransform(legacyAxisMatrix);
            dcc_import_transform::TransformCorrection legacyAxisCorrection;
            legacyAxisCorrection.translation = legacyAxisTransform.getTranslation(MSpace::kTransform);
            legacyAxisCorrection.rotation = legacyAxisTransform.rotation().asEulerRotation();
            documentCorrection.translation = legacyAxisCorrection.translation + documentCorrection.translation;
            documentCorrection.rotation = (legacyAxisCorrection.rotation.asQuaternion() *
                documentCorrection.rotation.asQuaternion()).asEulerRotation();
            maya_dmx::ReportWarning(axisWarning);
        }
    }
    AppendImportDebugLog("session: normalize import correction begin");
    NormalizeDocumentForImportCorrection(document_, importRoot_, documentCorrection);
    AppendImportDebugLog("session: normalize import correction ok");
    importOptions_.transformCorrection = dcc_import_transform::TransformCorrection();
    importOptions_.applyLegacyAxisCorrection = false;

    if (sceneMergeStrategy.usesUpdateCurrentScene())
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=update now reuses matching hierarchy, overwrites reused transforms/base animation, and attempts in-place mesh/deformer updates when matching nodes already exist; fine-grained scene-merge is still not implemented yet.");
    }
    else if (sceneMergeStrategy.usesAppendMissingObjects())
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=append currently reuses matching hierarchy and existing mesh carriers, but full scene-merge behavior is not implemented yet.");
    }
    else if (sceneMergeStrategy.usesAnimationOnlyImport())
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=animationOnly will only target matching existing scene nodes; missing DAG nodes, meshes and deformers are skipped.");
    }

    if (importOptions_.scenePolicy.importAnimationToLayer)
    {
        maya_dmx::ReportWarning("maya_dmx: importAnimationToLayer writes imported animation to a Maya override animation layer. Base animation remains unchanged while the layer is muted.");
    }
    if (sceneMergeStrategy.usesSourceDeltaImport())
    {
        maya_dmx::ReportWarning("maya_dmx: sourceDeltaMode applies Source-style subtract/linear-delta semantics to transform channels and writes the resulting delta to an additive Maya animation layer. Use Clip samples an existing scene animation layer; when it is disabled, the current scene state is used as the reference. Float channels remain absolute.");
    }

    AppendImportDebugLog("session: load document end success");
    return MStatus::kSuccess;
}

MStatus DmxImportSession::CreateSceneRoot(ImportContext &context, MObject &sceneRoot) const
{
    AppendImportDebugLog("session: create scene root begin");
    const MMatrix rootImportMatrix = BuildDmxTransformMatrix(document_, importRoot_);
    context.topLevelPreTransform = MMatrix::identity;
    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy))
    {
        sceneRoot = MObject::kNullObj;
        context.sceneRoot = sceneRoot;
        context.topLevelPreTransform = rootImportMatrix;
        AppendImportDebugLog("session: create scene root using existing scene root");
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform rootTransformFn;
    sceneRoot = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        AppendImportDebugLog("session: root transform create failed");
        return MStatus::kFailure;
    }

    rootTransformFn.setName(importRoot_->name.empty() ? "dmx_import" : importRoot_->name.c_str());
    AppendImportDebugLog((std::string("session: created scene root transform name=") + rootTransformFn.name().asChar()).c_str());

    status = ApplyTransform(document_, importRoot_, sceneRoot);
    if (!status)
    {
        AppendImportDebugLog("session: apply root transform failed");
        return MStatus::kFailure;
    }
    context.sceneRoot = sceneRoot;

    AppendImportDebugLog("session: create scene root end success");
    return MStatus::kSuccess;
}

MStatus DmxImportSession::ImportHierarchy(ImportContext &context, MObject sceneRoot) const
{
    const std::vector<const simple_dmx::Element *> children = FindAttributeElementArray(document_, importRoot_, "children");
    AppendImportDebugLog((std::string("session: import hierarchy begin childCount=") + std::to_string(children.size())).c_str());
    for (const simple_dmx::Element *child : children)
    {
        MStatus status = ImportDagHierarchyRecursive(context, child, sceneRoot);
        if (!status)
        {
            AppendImportDebugLog((std::string("session: hierarchy import failed child=") + (child ? child->name : "<null>")).c_str());
            return MStatus::kFailure;
        }
    }

    AppendImportDebugLog("session: hierarchy transforms imported");
    for (const simple_dmx::Element *child : children)
    {
        MStatus status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            AppendImportDebugLog((std::string("session: shape import failed child=") + (child ? child->name : "<null>")).c_str());
            return MStatus::kFailure;
        }
    }

    AppendImportDebugLog("session: import hierarchy end success");
    return MStatus::kSuccess;
}

MStatus DmxImportSession::ImportAnimation(std::shared_ptr<ImportContext> context, MObject sceneRoot)
{
    AppendImportDebugLog("session: import animation begin");

    AnimationImporter animator(context);
    animator.setLookupRoots(document_.GetRoot(), importRoot_, context->modelRoot);

    const simple_dmx::Element *combinationOperator = animator.FindCombinationOperator();
    MStatus status = animator.CreateCombinationControls(combinationOperator, sceneRoot);
    if (!status)
    {
        AppendImportDebugLog("session: create combination controls failed");
        return MStatus::kFailure;
    }
    AppendImportDebugLog((std::string("session: combination operator=") + (combinationOperator ? combinationOperator->name : "<null>")).c_str());

    const simple_dmx::Element *animationList = animator.FindAnimationList();
    if (!animationList)
    {
        AppendImportDebugLog("session: no animation list");
        return MStatus::kSuccess;
    }
    AppendImportDebugLog((std::string("session: animation list=") + animationList->name).c_str());

    const std::vector<const simple_dmx::Element *> animations =
        FindAttributeElementArray(document_, animationList, "animations");
    AppendImportDebugLog((std::string("session: animation clip count=") + std::to_string(animations.size())).c_str());
    for (const simple_dmx::Element *animation : animations)
    {
        AppendImportDebugLog((std::string("session: apply animation clip=") + (animation ? animation->name : "<null>")).c_str());
        status = animator.ApplyChannelsClipAnimation(animation);
        if (!status)
        {
            AppendImportDebugLog((std::string("session: apply animation clip failed=") + (animation ? animation->name : "<null>")).c_str());
            return MStatus::kFailure;
        }
    }

    AppendImportDebugLog("session: import animation end success");
    return MStatus::kSuccess;
}
