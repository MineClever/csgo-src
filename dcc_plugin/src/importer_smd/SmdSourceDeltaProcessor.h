#pragma once

#include "SmdImportSession.h"

#include <common/SceneMergeStrategy.h>

#include <vector>

#include <maya/MDagPath.h>
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
    const SmdImportOptions &importOptions_;
    const dcc_import_policy::SceneMergeResolver &mergeResolver_;
};
