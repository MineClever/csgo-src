#include "DmxImportSession.h"

#include "DmxImportAnimation.h"
#include "DmxImportDag.h"
#include "DmxImportInternals.h"
#include "DmxImportMeshMaterial.h"

#include <common/ImportTransformCorrection.h>
#include <common/SimpleDmxDocument.h>
#include <common/MayaDmxCommon.h>
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
std::string SanitizeLayerName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        {
            character = '_';
        }
    }

    return value.empty() ? std::string("dmx_delta") : value;
}

MMatrix ComputeLegacyAxisCorrectionMatrix(const std::string &sourceUpAxis, MString &warning)
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

std::string FormatVector3(const MVector &value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value.x << ' ' << value.y << ' ' << value.z;
    return stream.str();
}

std::string FormatQuaternion(const MQuaternion &value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value.x << ' ' << value.y << ' ' << value.z << ' ' << value.w;
    return stream.str();
}

void SetScalarStringAttribute(simple_dmx::Element &element, const char *attributeName, const std::string &fallbackType, const std::string &value)
{
    std::string declaredType = fallbackType;
    auto attributeIt = element.attributes.find(attributeName);
    if (attributeIt != element.attributes.end() && !attributeIt->second.declaredType.empty())
    {
        declaredType = attributeIt->second.declaredType;
    }

    simple_dmx::SetAttr(element, attributeName, simple_dmx::ScalarAttr(declaredType, value));
}

void SetStringArrayAttribute(
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
        SetScalarStringAttribute(transformElement, "position", "vector3", FormatVector3(correctedPosition));
    }
    else if (isTopLevelTransform && !correction.IsIdentity())
    {
        correctedPosition = dcc_import_transform::ApplyToTopLevelTranslation(correction, MVector::zero);
        SetScalarStringAttribute(transformElement, "position", "vector3", FormatVector3(correctedPosition));
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
    SetScalarStringAttribute(transformElement, "orientation", "quaternion", FormatQuaternion(correctedOrientation));
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

            correctedPositions.push_back(FormatVector3(
                dcc_import_transform::ApplyToTranslationScale(
                    scaleCorrection,
                    MVector(values[0], values[1], values[2]))));
        }
        SetStringArrayAttribute(vertexData, "positions", "vector3_array", std::move(correctedPositions));
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

            correctedNormals.push_back(FormatVector3(
                dcc_import_transform::ApplyToNormal(
                    scaleCorrection,
                    MVector(values[0], values[1], values[2]))));
        }
        SetStringArrayAttribute(vertexData, "normals", "vector3_array", std::move(correctedNormals));
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

        correctedDeltaPositions.push_back(FormatVector3(
            dcc_import_transform::ApplyToTranslationScale(
                scaleCorrection,
                MVector(values[0], values[1], values[2]))));
    }

    SetStringArrayAttribute(deltaState, "positions", "vector3_array", std::move(correctedDeltaPositions));
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
                    correctedValues.push_back(FormatVector3(correctedPosition));
                }
                SetStringArrayAttribute(*logLayer, "values", "vector3_array", std::move(correctedValues));
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

                    correctedValues.push_back(FormatQuaternion(
                        dcc_import_transform::ApplyToQuaternion(
                            correction,
                            MQuaternion(values[0], values[1], values[2], values[3]))));
                }
                SetStringArrayAttribute(*logLayer, "values", "quaternion_array", std::move(correctedValues));
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
    const simple_dmx::Element *animationList =
        FindAnimationList(document, documentRoot, importRoot, modelRoot);
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
    MStatus status = LoadDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    ImportContext context{document_};
    context.modelRoot = importRoot_->type == "DmeModel" ? importRoot_ : nullptr;
    context.scenePolicy = importOptions_.scenePolicy;
    context.importSkin = importOptions_.importSkin;
    context.importMaterials = importOptions_.importMaterials;
    context.importDeltaStates = importOptions_.importDeltaStates;
    if (context.modelRoot)
    {
        CollectJointInfo(document_, context.modelRoot, context);
    }

    MObject sceneRoot;
    status = CreateSceneRoot(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = ImportHierarchy(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = ImportAnimation(context, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    return maya_dmx::ReportInfo(MString("maya_dmx: imported hierarchy from ") + filePath_);
}

MStatus DmxImportSession::LoadDocument()
{
    const std::string fileText = ReadTextFile(filePath_);
    if (fileText.empty())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to read file ") + filePath_);
    }

    std::string parseError;
    if (!simple_dmx::ParseDocument(fileText, document_, parseError))
    {
        return maya_dmx::ReportError(MString("maya_dmx: parse error: ") + parseError.c_str());
    }

    importRoot_ = FindImportRoot(document_);
    if (!importRoot_)
    {
        return maya_dmx::ReportError("maya_dmx: no importable DMX root element found.");
    }

    importOptions_ = ParseImportOptions(optionsText_);
    dcc_import_policy::CaptureCurrentNamespace(importOptions_.scenePolicy);
    if (importOptions_.scenePolicy.forceDeltaAnimationLayer && importOptions_.scenePolicy.animationLayerName.empty())
    {
        const std::string resolvedPath = filePath_.asChar();
        const size_t lastSeparator = resolvedPath.find_last_of("/\\");
        const std::string baseName = lastSeparator == std::string::npos ? resolvedPath : resolvedPath.substr(lastSeparator + 1);
        importOptions_.scenePolicy.animationLayerName = SanitizeLayerName(baseName) + "_delta";
    }

    dcc_import_transform::TransformCorrection documentCorrection = importOptions_.transformCorrection;
    if (importOptions_.applyLegacyAxisCorrection)
    {
        MString axisWarning;
        const MMatrix legacyAxisMatrix = ComputeLegacyAxisCorrectionMatrix(FindAttributeString(importRoot_, "upAxis"), axisWarning);
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
    NormalizeDocumentForImportCorrection(document_, importRoot_, documentCorrection);
    importOptions_.transformCorrection = dcc_import_transform::TransformCorrection();
    importOptions_.applyLegacyAxisCorrection = false;

    if (dcc_import_policy::UsesUpdateCurrentScene(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=update now reuses matching hierarchy, overwrites reused transforms/base animation, and attempts in-place mesh/deformer updates when matching nodes already exist; fine-grained scene-merge is still not implemented yet.");
    }
    else if (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=append currently reuses matching hierarchy and existing mesh carriers, but full scene-merge behavior is not implemented yet.");
    }
    else if (dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=animationOnly is parsed but not implemented yet; falling back to create-new import behavior.");
    }

    if (importOptions_.scenePolicy.importAnimationToLayer && !importOptions_.scenePolicy.forceDeltaAnimationLayer)
    {
        maya_dmx::ReportWarning("maya_dmx: animation layer import options are parsed but not implemented yet; imported animation will still target the base scene.");
    }

    return MStatus::kSuccess;
}

MStatus DmxImportSession::CreateSceneRoot(ImportContext &context, MObject &sceneRoot) const
{
    const MMatrix rootImportMatrix = BuildDmxTransformMatrix(document_, importRoot_);
    context.topLevelPreTransform = MMatrix::identity;
    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy))
    {
        sceneRoot = MObject::kNullObj;
        context.sceneRoot = sceneRoot;
        context.topLevelPreTransform = rootImportMatrix;
        return MS::kSuccess;
    }

    MStatus status;
    MFnTransform rootTransformFn;
    sceneRoot = rootTransformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return MStatus::kFailure;
    }

    rootTransformFn.setName(importRoot_->name.empty() ? "dmx_import" : importRoot_->name.c_str());

    status = ApplyTransform(document_, importRoot_, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }
    context.sceneRoot = sceneRoot;

    return MStatus::kSuccess;
}

MStatus DmxImportSession::ImportHierarchy(ImportContext &context, MObject sceneRoot) const
{
    for (const simple_dmx::Element *child : FindAttributeElementArray(document_, importRoot_, "children"))
    {
        MStatus status = ImportDagHierarchyRecursive(context, child, sceneRoot);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    for (const simple_dmx::Element *child : FindAttributeElementArray(document_, importRoot_, "children"))
    {
        MStatus status = ImportDagShapesRecursive(context, child);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MStatus::kSuccess;
}

MStatus DmxImportSession::ImportAnimation(ImportContext &context, MObject sceneRoot) const
{
    const simple_dmx::Element *combinationOperator =
        FindCombinationOperator(document_, document_.GetRoot(), importRoot_, context.modelRoot);
    MStatus status = CreateCombinationControls(context, combinationOperator, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    const simple_dmx::Element *animationList =
        FindAnimationList(document_, document_.GetRoot(), importRoot_, context.modelRoot);
    if (!animationList)
    {
        return MStatus::kSuccess;
    }

    const std::vector<const simple_dmx::Element *> animations =
        FindAttributeElementArray(document_, animationList, "animations");
    for (const simple_dmx::Element *animation : animations)
    {
        status = ApplyChannelsClipAnimation(context, animation);
        if (!status)
        {
            return MStatus::kFailure;
        }
    }

    return MStatus::kSuccess;
}
