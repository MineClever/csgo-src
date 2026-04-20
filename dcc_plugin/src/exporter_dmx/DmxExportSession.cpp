#include "DmxExportSession.h"

#include "DmxExportAnimation.h"
#include "DmxExportDag.h"
#include "DmxExportInternals.h"

#include <common_dmx/MayaDmxCommon.h>
#include <common_dmx/SimpleDmxWrite.h>

#include <common/TransformCorrection.h>

#include <fstream>
#include <string>
#include <vector>

using dmx_export_translator::ExportContext;
using namespace dmx_export_impl;

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
