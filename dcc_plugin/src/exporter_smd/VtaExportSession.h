#pragma once

#include "SmdSceneExporter.h"

#include <common_smd/SimpleSmdDocument.h>

#include <maya/MFileObject.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <map>
#include <string>
#include <vector>

struct VtaExportTarget
{
    MObject blendShapeObject = MObject::kNullObj;
    MString blendShapeNodeName;
    unsigned int weightIndex = 0;
    int frameTime = 0;
};

struct VtaExportMeshBinding
{
    MDagPath meshPath;
    MDagPath transformPath;
    std::vector<int> rawToLocalVertexIndex;
    MPointArray basePoints;
    MFloatVectorArray baseNormals;
    std::vector<VtaExportTarget> targets;
};

class VtaExportSession
{
public:
    VtaExportSession(const MFileObject &fileObject, const MString &options, MPxFileTranslator::FileAccessMode mode);

    MStatus Run();

private:
    MStatus validateOutputFile() const;
    MStatus buildDocument();
    MStatus collectMeshBindings(std::vector<VtaExportMeshBinding> &bindings) const;
    MStatus appendVertexAnimationFrames(const std::vector<VtaExportMeshBinding> &bindings);
    MStatus appendBaseFrame(const std::vector<VtaExportMeshBinding> &bindings);
    MStatus appendTargetFrame(int frameTime, const std::vector<VtaExportMeshBinding> &bindings);
    MStatus serialize();
    MStatus writeOutput() const;

    MFileObject fileObject_;
    MString options_;
    MPxFileTranslator::FileAccessMode mode_;
    simple_smd::Document document_;
    std::string serialized_;
};
