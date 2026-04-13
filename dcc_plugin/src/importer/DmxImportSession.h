#pragma once

#include "DmxImportTranslatorTypes.h"

#include <common/SimpleDmxDocument.h>

#include <memory>
#include <maya/MFileObject.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

class DmxImportSession
{
public:
    DmxImportSession(const MFileObject &fileObject, const MString &options);

    MStatus Run();

private:
    MStatus LoadDocument();
    MStatus CreateSceneRoot(dmx_import_translator::ImportContext &context, MObject &sceneRoot) const;
    MStatus ImportHierarchy(dmx_import_translator::ImportContext &context, MObject sceneRoot) const;
    MStatus ImportAnimation(std::shared_ptr<dmx_import_translator::ImportContext> context, MObject sceneRoot);

    MString filePath_;
    MString optionsText_;
    simple_dmx::Document document_;
    const simple_dmx::Element *importRoot_ = nullptr;
    dmx_import_translator::ImportOptions importOptions_;
};
