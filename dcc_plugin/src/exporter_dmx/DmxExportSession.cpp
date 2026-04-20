#include "DmxExportSession.h"

#include "DmxExportAnimation.h"
#include "DmxExportDag.h"
#include "DmxExportInternals.h"

#include <common_dmx/MayaDmxCommon.h>
#include <common_dmx/SimpleDmxWrite.h>

#include <common/TransformCorrection.h>

#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MQuaternion.h>

#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

using dmx_export_translator::ExportContext;
using namespace dmx_export_impl;

namespace dmx_export_session_detail
{

using simple_dmx::Attribute;
using simple_dmx::Element;
using simple_dmx::FindAttributeString;
using simple_dmx::ParseNumberList;
using simple_dmx::ScalarArrayAttr;
using simple_dmx::ScalarAttr;
using simple_dmx::SetAttr;

Attribute *FindMutableAttribute(Element *element, const char *attributeName)
{
    if (!element)
    {
        return nullptr;
    }

    auto it = element->attributes.find(attributeName);
    return it != element->attributes.end() ? &it->second : nullptr;
}

Element *ResolveMutableElement(Attribute *attribute)
{
    if (!attribute || attribute->kind != Attribute::Kind::Element || !attribute->elementValue.inlineElement)
    {
        return nullptr;
    }

    return attribute->elementValue.inlineElement.get();
}

std::vector<Element *> ResolveMutableElementArray(Attribute *attribute)
{
    std::vector<Element *> resolved;
    if (!attribute || attribute->kind != Attribute::Kind::ElementArray)
    {
        return resolved;
    }

    resolved.reserve(attribute->elementArray.size());
    for (simple_dmx::ElementLink &link : attribute->elementArray)
    {
        resolved.push_back(link.inlineElement ? link.inlineElement.get() : nullptr);
    }
    return resolved;
}

bool ParseVector3String(const std::string &value, MVector &parsed)
{
    const std::vector<double> values = ParseNumberList(value);
    if (values.size() < 3)
    {
        return false;
    }

    parsed = MVector(values[0], values[1], values[2]);
    return true;
}

bool ParseQuaternionString(const std::string &value, MQuaternion &parsed)
{
    const std::vector<double> values = ParseNumberList(value);
    if (values.size() < 4)
    {
        return false;
    }

    parsed = MQuaternion(values[0], values[1], values[2], values[3]);
    return true;
}

bool ParseVector4String(const std::string &value, MVector &parsed, double &w)
{
    const std::vector<double> values = ParseNumberList(value);
    if (values.size() < 4)
    {
        return false;
    }

    parsed = MVector(values[0], values[1], values[2]);
    w = values[3];
    return true;
}

bool ParseMatrixStringLocal(const std::string &value, MMatrix &parsed)
{
    const std::vector<double> values = ParseNumberList(value);
    if (values.size() < 16)
    {
        return false;
    }

    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            parsed[row][column] = values[row * 4 + column];
        }
    }
    return true;
}

MVector ApplyLinearVectorCorrection(
    const dcc_export_transform::ExportTransformPolicy &policy,
    const MVector &vector)
{
    const MVector zeroPoint = dcc_export_transform::ApplyToPoint(policy, MVector::zero);
    return dcc_export_transform::ApplyToPoint(policy, vector) - zeroPoint;
}

void CorrectVector3Array(
    Element *element,
    const char *attributeName,
    const dcc_export_transform::ExportTransformPolicy &policy,
    bool useDirectionTransform,
    bool useScaleOnly)
{
    Attribute *attribute = FindMutableAttribute(element, attributeName);
    if (!attribute || attribute->kind != Attribute::Kind::StringArray)
    {
        return;
    }

    std::vector<std::string> correctedValues;
    correctedValues.reserve(attribute->stringArray.size());
    for (const std::string &value : attribute->stringArray)
    {
        MVector parsed;
        if (!ParseVector3String(value, parsed))
        {
            correctedValues.push_back(value);
            continue;
        }

        MVector corrected = parsed;
        if (useDirectionTransform)
        {
            corrected = dcc_export_transform::ApplyToDirection(policy, parsed);
        }
        else if (useScaleOnly)
        {
            corrected = dcc_transform::ApplyToTranslationScale(policy.correction, parsed);
        }
        else
        {
            corrected = dcc_export_transform::ApplyToPoint(policy, parsed);
        }
        correctedValues.push_back(FormatVector3(corrected.x, corrected.y, corrected.z));
    }

    SetAttr(*element, attributeName, ScalarArrayAttr(attribute->declaredType, std::move(correctedValues)));
}

void CorrectDeltaPositionArray(
    Element *deltaElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    Attribute *attribute = FindMutableAttribute(deltaElement, "positions");
    if (!attribute || attribute->kind != Attribute::Kind::StringArray)
    {
        return;
    }

    std::vector<std::string> correctedValues;
    correctedValues.reserve(attribute->stringArray.size());
    for (const std::string &value : attribute->stringArray)
    {
        MVector parsed;
        if (!ParseVector3String(value, parsed))
        {
            correctedValues.push_back(value);
            continue;
        }

        const MVector corrected = dcc_transform::ApplyToTranslationScale(policy.correction, parsed);
        correctedValues.push_back(FormatVector3(corrected.x, corrected.y, corrected.z));
    }

    SetAttr(*deltaElement, "positions", ScalarArrayAttr(attribute->declaredType, std::move(correctedValues)));
}

void CorrectVector4Array(
    Element *element,
    const char *attributeName,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    Attribute *attribute = FindMutableAttribute(element, attributeName);
    if (!attribute || attribute->kind != Attribute::Kind::StringArray)
    {
        return;
    }

    std::vector<std::string> correctedValues;
    correctedValues.reserve(attribute->stringArray.size());
    for (const std::string &value : attribute->stringArray)
    {
        MVector parsed;
        double w = 1.0;
        if (!ParseVector4String(value, parsed, w))
        {
            correctedValues.push_back(value);
            continue;
        }

        const MVector corrected = dcc_export_transform::ApplyToDirection(policy, parsed);
        correctedValues.push_back(FormatVector4(corrected.x, corrected.y, corrected.z, w));
    }

    SetAttr(*element, attributeName, ScalarArrayAttr(attribute->declaredType, std::move(correctedValues)));
}

void CorrectMatrixMetadata(
    Element *vertexDataElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    Attribute *bindPreMatrixAttribute = FindMutableAttribute(vertexDataElement, "mayaBindPreMatrix");
    if (!bindPreMatrixAttribute || bindPreMatrixAttribute->kind != Attribute::Kind::StringArray)
    {
        return;
    }

    const MMatrix inverseCorrection = policy.correction.Matrix().inverse();
    std::vector<std::string> correctedValues;
    correctedValues.reserve(bindPreMatrixAttribute->stringArray.size());
    for (const std::string &value : bindPreMatrixAttribute->stringArray)
    {
        MMatrix parsedMatrix;
        if (!ParseMatrixStringLocal(value, parsedMatrix))
        {
            correctedValues.push_back(value);
            continue;
        }

        correctedValues.push_back(FormatMatrix(inverseCorrection * parsedMatrix));
    }

    SetAttr(
        *vertexDataElement,
        "mayaBindPreMatrix",
        ScalarArrayAttr(bindPreMatrixAttribute->declaredType, std::move(correctedValues)));
}

void CorrectVertexDataElement(
    Element *vertexDataElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    if (!vertexDataElement)
    {
        return;
    }

    CorrectVector3Array(vertexDataElement, "positions", policy, false, true);
    CorrectVector3Array(vertexDataElement, "normals", policy, true, false);
    CorrectVector4Array(vertexDataElement, "tangents", policy);
    CorrectMatrixMetadata(vertexDataElement, policy);
}

void CorrectMeshElement(
    Element *meshElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    if (!meshElement)
    {
        return;
    }

    CorrectVertexDataElement(ResolveMutableElement(FindMutableAttribute(meshElement, "bindState")), policy);
    for (Element *baseStateElement : ResolveMutableElementArray(FindMutableAttribute(meshElement, "baseStates")))
    {
        CorrectVertexDataElement(baseStateElement, policy);
    }
    CorrectVertexDataElement(ResolveMutableElement(FindMutableAttribute(meshElement, "currentState")), policy);

    for (Element *deltaElement : ResolveMutableElementArray(FindMutableAttribute(meshElement, "deltaStates")))
    {
        if (deltaElement)
        {
            CorrectDeltaPositionArray(deltaElement, policy);
        }
    }
}

void CorrectTopLevelTransformElement(
    Element *transformElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    if (!transformElement)
    {
        return;
    }

    MVector position;
    if (ParseVector3String(FindAttributeString(transformElement, "position"), position))
    {
        const MVector corrected = dcc_export_transform::ApplyToTopLevelTranslation(policy, position);
        SetAttr(*transformElement, "position", ScalarAttr("vector3", FormatVector3(corrected.x, corrected.y, corrected.z)));
    }

    MQuaternion orientation;
    if (ParseQuaternionString(FindAttributeString(transformElement, "orientation"), orientation))
    {
        const MQuaternion corrected = dcc_export_transform::ApplyToTopLevelQuaternion(policy, orientation);
        SetAttr(
            *transformElement,
            "orientation",
            ScalarAttr("quaternion", FormatQuaternion(corrected.x, corrected.y, corrected.z, corrected.w)));
    }
}

void CorrectChildTransformElement(
    Element *transformElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    if (!transformElement)
    {
        return;
    }

    MVector position;
    if (ParseVector3String(FindAttributeString(transformElement, "position"), position))
    {
        const MVector corrected = dcc_transform::ApplyToTranslationScale(policy.correction, position);
        SetAttr(*transformElement, "position", ScalarAttr("vector3", FormatVector3(corrected.x, corrected.y, corrected.z)));
    }
}

void CorrectDagElementsRecursive(
    Element *dagElement,
    const dcc_export_transform::ExportTransformPolicy &policy,
    bool isTopLevelDag)
{
    if (!dagElement)
    {
        return;
    }

    if (isTopLevelDag)
    {
        CorrectTopLevelTransformElement(ResolveMutableElement(FindMutableAttribute(dagElement, "transform")), policy);
    }
    else
    {
        CorrectChildTransformElement(ResolveMutableElement(FindMutableAttribute(dagElement, "transform")), policy);
    }

    CorrectMeshElement(ResolveMutableElement(FindMutableAttribute(dagElement, "shape")), policy);
    for (Element *child : ResolveMutableElementArray(FindMutableAttribute(dagElement, "children")))
    {
        CorrectDagElementsRecursive(child, policy, false);
    }
}

void CorrectAnimationLayerValues(
    Element *logLayerElement,
    const dcc_export_transform::ExportTransformPolicy &policy,
    bool isQuaternion)
{
    Attribute *valuesAttribute = FindMutableAttribute(logLayerElement, "values");
    if (!valuesAttribute || valuesAttribute->kind != Attribute::Kind::StringArray)
    {
        return;
    }

    std::vector<std::string> correctedValues;
    correctedValues.reserve(valuesAttribute->stringArray.size());
    for (const std::string &value : valuesAttribute->stringArray)
    {
        if (isQuaternion)
        {
            MQuaternion parsed;
            if (!ParseQuaternionString(value, parsed))
            {
                correctedValues.push_back(value);
                continue;
            }

            const MQuaternion corrected = dcc_export_transform::ApplyToTopLevelQuaternion(policy, parsed);
            correctedValues.push_back(FormatQuaternion(corrected.x, corrected.y, corrected.z, corrected.w));
            continue;
        }

        MVector parsed;
        if (!ParseVector3String(value, parsed))
        {
            correctedValues.push_back(value);
            continue;
        }

        const MVector corrected = dcc_export_transform::ApplyToTopLevelTranslation(policy, parsed);
        correctedValues.push_back(FormatVector3(corrected.x, corrected.y, corrected.z));
    }

    SetAttr(*logLayerElement, "values", ScalarArrayAttr(valuesAttribute->declaredType, std::move(correctedValues)));
}

void CorrectAnimationChannels(
    Element *animationListElement,
    const std::unordered_set<Element *> &topLevelTransformElements,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    if (!animationListElement)
    {
        return;
    }

    for (Element *clipElement : ResolveMutableElementArray(FindMutableAttribute(animationListElement, "animations")))
    {
        if (!clipElement)
        {
            continue;
        }

        for (Element *channelElement : ResolveMutableElementArray(FindMutableAttribute(clipElement, "channels")))
        {
            if (!channelElement)
            {
                continue;
            }

            Element *targetElement = ResolveMutableElement(FindMutableAttribute(channelElement, "toElement"));
            if (!targetElement || topLevelTransformElements.find(targetElement) == topLevelTransformElements.end())
            {
                continue;
            }

            const std::string targetAttribute = FindAttributeString(channelElement, "toAttribute");
            const bool correctPosition = targetAttribute == "position";
            const bool correctOrientation = targetAttribute == "orientation";
            if (!correctPosition && !correctOrientation)
            {
                continue;
            }

            Element *logElement = ResolveMutableElement(FindMutableAttribute(channelElement, "log"));
            if (!logElement)
            {
                continue;
            }

            for (Element *layerElement : ResolveMutableElementArray(FindMutableAttribute(logElement, "layers")))
            {
                if (!layerElement)
                {
                    continue;
                }

                if (correctOrientation)
                {
                    CorrectAnimationLayerValues(layerElement, policy, true);
                    continue;
                }

                Attribute *valuesAttribute = FindMutableAttribute(layerElement, "values");
                if (!valuesAttribute || valuesAttribute->kind != Attribute::Kind::StringArray)
                {
                    continue;
                }

                std::vector<std::string> correctedValues;
                correctedValues.reserve(valuesAttribute->stringArray.size());
                for (const std::string &value : valuesAttribute->stringArray)
                {
                    MVector parsed;
                    if (!ParseVector3String(value, parsed))
                    {
                        correctedValues.push_back(value);
                        continue;
                    }

                    const MVector corrected = topLevelTransformElements.find(targetElement) != topLevelTransformElements.end() ?
                        dcc_export_transform::ApplyToTopLevelTranslation(policy, parsed) :
                        dcc_transform::ApplyToTranslationScale(policy.correction, parsed);
                    correctedValues.push_back(FormatVector3(corrected.x, corrected.y, corrected.z));
                }
                SetAttr(*layerElement, "values", ScalarArrayAttr(valuesAttribute->declaredType, std::move(correctedValues)));
            }
        }
    }
}

} // namespace dmx_export_session_detail

DmxExportSession::DmxExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode)
    : fileObject_(fileObject)
    , optionsText_(options)
    , mode_(mode)
{
}

MStatus DmxExportSession::Run()
{
    AppendDebugLog("writer: begin");

    MStatus status = Initialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = BuildDocument();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = Serialize();
    if (!status)
    {
        return MStatus::kFailure;
    }

    status = WriteOutput();
    if (!status)
    {
        return MStatus::kFailure;
    }

    AppendDebugLog("writer: wrote file");
    return maya_dmx::ReportInfo(
        MString(exportOptions_.binary ? "maya_dmx: exported binary DMX to " : "maya_dmx: exported text DMX to ") +
        fileObject_.rawFullName());
}

MStatus DmxExportSession::Initialize()
{
    exportOptions_ = ParseExportOptions(fileObject_, optionsText_);
    exportRoots_ = CollectExportRoots(mode_);
    AppendDebugLog("writer: collected roots");
    if (exportRoots_.empty())
    {
        AppendDebugLog("writer: no roots");
        return maya_dmx::ReportError("maya_dmx: nothing to export.");
    }

    return MStatus::kSuccess;
}

MStatus DmxExportSession::BuildDocument()
{
    simple_dmx::DocumentBuilder builder;
    ExportContext context;
    context.exportSkin = exportOptions_.exportSkin;
    context.exportDeltaStates = exportOptions_.exportDeltaStates;
    context.exportMetadata = exportOptions_.exportMetadata;
    context.materialRoot = exportOptions_.materialRoot;
    context.transformPolicy = dcc_export_transform::BuildExportTransformPolicy(exportOptions_.transformCorrection);
    for (const MDagPath &rootPath : exportRoots_)
    {
        context.topLevelDagPaths.insert(DagPathKey(rootPath));
    }

    simple_dmx::Element *modelElement = builder.CreateElement("DmeModel", "maya_export");
    SetAttr(*modelElement, "upAxis", ScalarAttr("string", exportOptions_.upAxis));
    if (exportOptions_.exportMetadata && !exportOptions_.materialRoot.empty())
    {
        SetAttr(*modelElement, "mayaMaterialRoot", ScalarAttr("string", exportOptions_.materialRoot));
    }

    std::vector<simple_dmx::Element *> rootChildren;
    for (const MDagPath &rootPath : exportRoots_)
    {
        RegisterDagElementsRecursive(builder, rootPath, context);
    }
    for (const MDagPath &rootPath : exportRoots_)
    {
        if (simple_dmx::Element *child = BuildDagElement(builder, rootPath, context))
        {
            rootChildren.push_back(child);
        }
    }
    AppendDebugLog("writer: built dag elements");

    if (!rootChildren.empty())
    {
        SetAttr(*modelElement, "children", builder.ElementRefArray(rootChildren));
    }
    if (!context.jointElements.empty())
    {
        SetAttr(*modelElement, "jointList", builder.ElementRefArray(context.jointElements));
    }
    if (simple_dmx::Element *animationListElement = BuildAnimationListElement(builder, exportRoots_, context))
    {
        SetAttr(*modelElement, "animationList", builder.ElementRef(animationListElement));
    }

    MStatus status = ApplyDocumentTransformCorrection(modelElement);
    if (!status)
    {
        return MStatus::kFailure;
    }

    builder.SetRoot(modelElement);
    const simple_dmx::Document document = builder.Build();

    std::string serializeError;
    if (exportOptions_.binary)
    {
        if (!simple_dmx::SerializeDocumentBinary(document, serialized_, serializeError))
        {
            AppendDebugLog("writer: binary serialize failed");
            return maya_dmx::ReportError(serializeError.c_str());
        }
    }
    else
    {
        serialized_ = simple_dmx::SerializeDocumentText(document);
    }

    AppendDebugLog("writer: serialized");
    return MStatus::kSuccess;
}

MStatus DmxExportSession::ApplyDocumentTransformCorrection(simple_dmx::Element *modelElement) const
{
    const dcc_export_transform::ExportTransformPolicy policy =
        dcc_export_transform::BuildExportTransformPolicy(exportOptions_.transformCorrection);
    if (!modelElement || policy.IsIdentity())
    {
        return modelElement ? MStatus::kSuccess : MStatus::kFailure;
    }

    using namespace dmx_export_session_detail;

    std::unordered_set<simple_dmx::Element *> topLevelTransformElements;
    for (simple_dmx::Element *rootChild : ResolveMutableElementArray(FindMutableAttribute(modelElement, "children")))
    {
        if (!rootChild)
        {
            continue;
        }

        simple_dmx::Element *transformElement = ResolveMutableElement(FindMutableAttribute(rootChild, "transform"));
        if (transformElement)
        {
            topLevelTransformElements.insert(transformElement);
        }

        CorrectDagElementsRecursive(rootChild, policy, true);
    }

    CorrectAnimationChannels(
        ResolveMutableElement(FindMutableAttribute(modelElement, "animationList")),
        topLevelTransformElements,
        policy);
    return MStatus::kSuccess;
}

MStatus DmxExportSession::Serialize()
{
    return MStatus::kSuccess;
}

MStatus DmxExportSession::WriteOutput() const
{
    std::ofstream output(fileObject_.rawFullName().asChar(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to open output file ") + fileObject_.rawFullName());
    }

    output.write(serialized_.data(), static_cast<std::streamsize>(serialized_.size()));
    output.close();
    if (!output)
    {
        return maya_dmx::ReportError(MString("maya_dmx: failed to write output file ") + fileObject_.rawFullName());
    }

    return MStatus::kSuccess;
}
