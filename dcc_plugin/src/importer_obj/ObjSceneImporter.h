#pragma once

#include "ObjImportSession.h"

#include <common/ImportPolicy.h>
#include <common/SceneMergeStrategy.h>
#include <common_obj/SimpleObjDocument.h>

#include <memory>
#include <string>
#include <unordered_map>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

class ObjSceneImporter
{
public:
    ObjSceneImporter(
        std::shared_ptr<const simple_obj::Document> document,
        const ObjImportOptions &importOptions);

    MStatus Import();

private:
    MStatus createImportRoot();
    MStatus importShape(const rapidobj::Shape &shape);
    MStatus createMayaMesh(
        const rapidobj::Attributes &attributes,
        const rapidobj::Mesh &mesh,
        const std::string &shapeName,
        MObject &outTransformObj);
    MStatus assignDefaultMaterial(const MObject &meshTransformObj);
    MStatus assignPerFaceMaterials(
        const rapidobj::Mesh &mesh,
        const rapidobj::Materials &materials,
        const MObject &meshTransformObj);
    MString sanitizeNodeName(const std::string &name) const;

    std::shared_ptr<const simple_obj::Document> document_;
    ObjImportOptions importOptions_;
    dcc_import_policy::SceneMergeResolver mergeResolver_;
    MObject importRoot_ = MObject::kNullObj;
};
