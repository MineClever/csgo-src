#pragma once

#include "DmxImportTranslatorTypes.h"

#include "../common/SimpleDmxDocument.h"

#include <maya/MFileObject.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

class ImportSession
{
public:
    ImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus LoadDocument();
    MStatus CreateSceneRoot(dmx_import_translator::ImportContext &context, MObject &sceneRoot) const;
    MStatus ImportHierarchy(dmx_import_translator::ImportContext &context, MObject sceneRoot) const;
    MStatus ImportAnimation(dmx_import_translator::ImportContext &context, MObject sceneRoot) const;

    MString filePath_;
    MString optionsText_;
    simple_dmx::Document document_;
    const simple_dmx::Element *importRoot_ = nullptr;
    dmx_import_translator::ImportOptions importOptions_;
};
