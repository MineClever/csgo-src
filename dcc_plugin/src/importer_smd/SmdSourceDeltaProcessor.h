#pragma once

#include "SmdImportSession.h"

#include <common/SceneMergeStrategy.h>

#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>

class SmdSourceDeltaProcessor
{
public:
    SmdSourceDeltaProcessor(
        const SmdImportOptions &importOptions,
        const dcc_import_policy::SceneMergeResolver &mergeResolver);

    MStatus ApplyToSamples(
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;

private:
    MStatus buildSceneReferenceSamples(
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;
    MStatus buildSceneLayerSamples(
        const MString &layerName,
        const MDagPath &jointPath,
        const std::vector<double> &times,
        std::vector<MVector> &translations,
        std::vector<MQuaternion> &rotations) const;
    MStatus sampleLayerPlugValue(
        const MString &layerName,
        const MPlug &plug,
        double time,
        double &value) const;

    const SmdImportOptions &importOptions_;
    const dcc_import_policy::SceneMergeResolver &mergeResolver_;
};
