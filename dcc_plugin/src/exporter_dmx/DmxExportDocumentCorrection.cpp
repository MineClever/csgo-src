#include "DmxExportDocumentCorrection.h"

#include "DmxExportInternals.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

namespace dmx_export_impl
{
namespace dmx_export_document_correction_detail
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
            corrected = dcc_export_transform::ApplyToLocalNormal(policy, parsed);
        }
        else if (useScaleOnly)
        {
            corrected = dcc_export_transform::ApplyToLocalTranslation(policy, parsed);
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

        const MVector corrected = dcc_export_transform::ApplyToLocalTranslation(policy, parsed);
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

        const MVector corrected = dcc_export_transform::ApplyToLocalTangent(policy, parsed);
        correctedValues.push_back(
            FormatVector4(corrected.x, corrected.y, corrected.z, dcc_export_transform::ApplyToLocalTangentHandedness(policy, w)));
    }

    SetAttr(*element, attributeName, ScalarArrayAttr(attribute->declaredType, std::move(correctedValues)));
}

void CorrectMatrixMetadata(Element *vertexDataElement)
{
    vertexDataElement->attributes.erase("mayaGeomMatrix");
    vertexDataElement->attributes.erase("mayaBindPreMatrix");
    vertexDataElement->attributeOrder.erase(
        std::remove(vertexDataElement->attributeOrder.begin(), vertexDataElement->attributeOrder.end(), "mayaGeomMatrix"),
        vertexDataElement->attributeOrder.end());
    vertexDataElement->attributeOrder.erase(
        std::remove(vertexDataElement->attributeOrder.begin(), vertexDataElement->attributeOrder.end(), "mayaBindPreMatrix"),
        vertexDataElement->attributeOrder.end());
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
    CorrectMatrixMetadata(vertexDataElement);
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
        const MVector corrected = dcc_export_transform::ApplyToLocalTranslation(policy, position);
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
                        dcc_export_transform::ApplyToLocalTranslation(policy, parsed);
                    correctedValues.push_back(FormatVector3(corrected.x, corrected.y, corrected.z));
                }
                SetAttr(*layerElement, "values", ScalarArrayAttr(valuesAttribute->declaredType, std::move(correctedValues)));
            }
        }
    }
}

} // namespace dmx_export_document_correction_detail

MStatus ApplyDocumentTransformCorrection(
    simple_dmx::Element *modelElement,
    const dcc_export_transform::ExportTransformPolicy &policy)
{
    using namespace dmx_export_document_correction_detail;

    if (!modelElement || policy.IsIdentity())
    {
        return modelElement ? MStatus::kSuccess : MStatus::kFailure;
    }

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

} // namespace dmx_export_impl
