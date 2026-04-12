#pragma once

// Internal use only — included exclusively by sub-module .cpp files in the importer.
// Do NOT include this header from other .h files.

#include "DmxImportInternals.h"

#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{

MStatus ApplyTransform(
    const simple_dmx::Document &document,
    const simple_dmx::Element *dagElement,
    MObject object,
    const MMatrix &preTransform = MMatrix::identity);
MObject CreateDagNode(const std::string &name, bool isJoint, MObject parent, MStatus &status);
void CollectJointInfo(
    const simple_dmx::Document &document,
    const simple_dmx::Element *modelElement,
    ImportContext &context);
MStatus ImportDagHierarchyRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement,
    MObject parent);
MStatus ImportDagShapesRecursive(
    ImportContext &context,
    const simple_dmx::Element *dagElement);
const simple_dmx::Element *FindImportRoot(const simple_dmx::Document &document);

} // namespace dmx_import_impl
