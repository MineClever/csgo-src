#include "WorkflowExecutor.h"

#include <common_dmx/MayaDmxCommon.h>
#include "BatchManifestStore.h"
#include "WorkflowSupport.h"

#include <maya/MFileIO.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>

#include <filesystem>
#include <sstream>

namespace maya_dmx
{
namespace detail
{
MString FormatOptionDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str().c_str();
}
} // namespace detail

MString WorkflowExecutor::BuildTranslatorOptions(const ExportPreset &preset) const
{
    MString options;
    options += "encoding=";
    options += preset.dmxEncoding.length() == 0 ? "text" : preset.dmxEncoding;
    options += ";upAxis=";
    options += preset.upAxis.length() == 0 ? "Y" : preset.upAxis;
    options += ";exportSkin=";
    options += preset.exportSkin ? "1" : "0";
    options += ";exportDeltaStates=";
    options += preset.exportDeltaStates ? "1" : "0";
    options += ";exportMetadata=";
    options += preset.exportMetadata ? "1" : "0";
    if (preset.materialRoot.length() > 0)
    {
        options += ";materialRoot=";
        options += preset.materialRoot;
    }
    options += ";translateX=";
    options += detail::FormatOptionDouble(preset.translateX);
    options += ";translateY=";
    options += detail::FormatOptionDouble(preset.translateY);
    options += ";translateZ=";
    options += detail::FormatOptionDouble(preset.translateZ);
    options += ";rotateX=";
    options += detail::FormatOptionDouble(preset.rotateX);
    options += ";rotateY=";
    options += detail::FormatOptionDouble(preset.rotateY);
    options += ";rotateZ=";
    options += detail::FormatOptionDouble(preset.rotateZ);
    options += ";scaleX=";
    options += detail::FormatOptionDouble(preset.scaleX);
    options += ";scaleY=";
    options += detail::FormatOptionDouble(preset.scaleY);
    options += ";scaleZ=";
    options += detail::FormatOptionDouble(preset.scaleZ);
    return options;
}

MStatus WorkflowExecutor::ExecuteExport(const ExportPreset &preset, const MString &outputPath, bool exportSelection) const
{
    if (outputPath.length() == 0)
    {
        return ReportError("maya_dmx: output path is required.");
    }

    const MString normalizedOutputPath = std::filesystem::path(outputPath.asChar()).lexically_normal().generic_string().c_str();

    MStatus status = workflow_support::EnsureParentDirectory(normalizedOutputPath);
    if (!status)
    {
        return MStatus::kFailure;
    }

    {
        std::error_code errorCode;
        std::filesystem::remove(std::filesystem::path(normalizedOutputPath.asChar()), errorCode);
    }

    MString previousOptions;
    const bool hadPreviousOptions = MGlobal::optionVarExists("FileTranslatorOptions");
    if (hadPreviousOptions)
    {
        previousOptions = MGlobal::optionVarStringValue("FileTranslatorOptions");
    }

    MGlobal::setOptionVarValue("FileTranslatorOptions", BuildTranslatorOptions(preset));
    status = exportSelection ?
        MFileIO::exportSelected(normalizedOutputPath, "Source DMX Export", false) :
        MFileIO::exportAll(normalizedOutputPath, "Source DMX Export", false);

    if (hadPreviousOptions)
    {
        MGlobal::setOptionVarValue("FileTranslatorOptions", previousOptions);
    }
    else
    {
        MGlobal::removeOptionVar("FileTranslatorOptions");
    }

    if (!status)
    {
        return ReportError(MString("maya_dmx: workflow export failed for ") + normalizedOutputPath, status);
    }

    return MS::kSuccess;
}

MStatus WorkflowExecutor::ExecuteBatchExport(const ExportPreset &preset, const MStringArray &entries) const
{
    if (entries.length() == 0)
    {
        return ReportError("maya_dmx: batch manifest has no entries.");
    }

    BatchManifestStore store;
    MSelectionList originalSelection;
    MGlobal::getActiveSelectionList(originalSelection);

    for (unsigned int entryIndex = 0; entryIndex < entries.length(); ++entryIndex)
    {
        BatchManifestEntry entry;
        if (!store.ParseBatchManifestEntry(entries[entryIndex], entry))
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return ReportError(
                MString("maya_dmx: invalid batch entry at index ")
                + static_cast<int>(entryIndex)
                + ": " + entries[entryIndex]);
        }

        MSelectionList exportSelection;
        MStatus status = exportSelection.add(entry.rootPath);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return ReportError(
                MString("maya_dmx: batch root was not found for entry ")
                + static_cast<int>(entryIndex)
                + ": " + entry.rootPath,
                status);
        }

        status = MGlobal::setActiveSelectionList(exportSelection);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return MStatus::kFailure;
        }

        status = ExecuteExport(preset, workflow_support::JoinPath(preset.outputDirectory, entry.outputPath), true);
        if (!status)
        {
            MGlobal::setActiveSelectionList(originalSelection);
            return MStatus::kFailure;
        }
    }

    MGlobal::setActiveSelectionList(originalSelection);
    return MS::kSuccess;
}
}
