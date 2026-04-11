#include "DmxImportSession.h"

#include "DmxImportAnimation.h"
#include "DmxImportDag.h"
#include "DmxImportInternals.h"

#include <common/MayaDmxCommon.h>
#include <common/SimpleDmxText.h>

#include <string>
#include <vector>

#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>
using simple_dmx::FindAttributeElementArray;
using simple_dmx::FindAttributeString;
using dmx_import_translator::ImportContext;
using namespace dmx_import_impl;

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
    context.applyAxisCorrection = importOptions_.applyAxisCorrection;
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

    if (dcc_import_policy::UsesUpdateCurrentScene(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=update is parsed but not implemented yet; falling back to create-new import behavior.");
    }
    else if (dcc_import_policy::UsesAppendMissingObjects(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=append currently reuses matching hierarchy and existing mesh carriers, but full scene-merge behavior is not implemented yet.");
    }
    else if (dcc_import_policy::UsesAnimationOnlyImport(importOptions_.scenePolicy))
    {
        maya_dmx::ReportWarning("maya_dmx: importMode=animationOnly is parsed but not implemented yet; falling back to create-new import behavior.");
    }

    if (importOptions_.scenePolicy.importAnimationToLayer)
    {
        maya_dmx::ReportWarning("maya_dmx: animation layer import options are parsed but not implemented yet; imported animation will still target the base scene.");
    }

    return MStatus::kSuccess;
}

MStatus DmxImportSession::CreateSceneRoot(ImportContext &context, MObject &sceneRoot) const
{
    if (dcc_import_policy::UsesSceneRoot(importOptions_.scenePolicy))
    {
        sceneRoot = MObject::kNullObj;
        if (context.applyAxisCorrection)
        {
            maya_dmx::ReportWarning("maya_dmx: skipped root axis correction because useSceneRoot=1 imports directly into the Maya scene.");
        }
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

    const std::string upAxis = FindAttributeString(importRoot_, "upAxis");
    MEulerRotation rootAxisCorrection;
    MString rootAxisWarning;
    const bool needsAxisCorrection =
        context.applyAxisCorrection &&
        ComputeRootAxisCorrection(upAxis, rootAxisCorrection, rootAxisWarning);

    status = ApplyTransform(document_, importRoot_, sceneRoot);
    if (!status)
    {
        return MStatus::kFailure;
    }

    if (needsAxisCorrection)
    {
        status = rootTransformFn.setRotation(rootAxisCorrection.asQuaternion());
        if (!status)
        {
            return MStatus::kFailure;
        }
        maya_dmx::ReportWarning(rootAxisWarning);
    }

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
