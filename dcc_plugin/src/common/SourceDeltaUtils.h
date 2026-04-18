#pragma once

#include "ImportPolicy.h"

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <vector>

#include <maya/MQuaternion.h>
#include <maya/MVector.h>

namespace dcc_source_delta
{

inline double ApplySplineWeight(double t)
{
    return 3.0 * t * t - 2.0 * t * t * t;
}

inline MVector ApplySourceDeltaDifference(
    const MVector &value,
    const MVector &reference,
    dcc_import_policy::SourceDeltaMode mode)
{
    return mode == dcc_import_policy::SourceDeltaMode::PreSubtract ?
        reference - value :
        value - reference;
}

inline MQuaternion ApplySourceDeltaDifference(
    const MQuaternion &value,
    const MQuaternion &reference,
    dcc_import_policy::SourceDeltaMode mode)
{
    return mode == dcc_import_policy::SourceDeltaMode::PreSubtract ?
        reference.inverse() * value :
        value * reference.inverse();
}

inline void ApplySourceDeltaReferenceSamples(
    std::vector<MVector> &samples,
    const std::vector<MVector> &referenceSamples,
    dcc_import_policy::SourceDeltaMode mode)
{
    if (samples.empty() || samples.size() != referenceSamples.size())
    {
        return;
    }

    for (size_t index = 0; index < samples.size(); ++index)
    {
        samples[index] = ApplySourceDeltaDifference(samples[index], referenceSamples[index], mode);
    }
}

inline void ApplySourceDeltaReferenceSamples(
    std::vector<MQuaternion> &samples,
    const std::vector<MQuaternion> &referenceSamples,
    dcc_import_policy::SourceDeltaMode mode)
{
    if (samples.empty() || samples.size() != referenceSamples.size())
    {
        return;
    }

    for (size_t index = 0; index < samples.size(); ++index)
    {
        samples[index] = ApplySourceDeltaDifference(samples[index], referenceSamples[index], mode);
    }
}

inline void ApplySourceDeltaReferenceValue(
    std::vector<MVector> &samples,
    const MVector &referenceValue,
    dcc_import_policy::SourceDeltaMode mode)
{
    for (MVector &sampleValue : samples)
    {
        sampleValue = ApplySourceDeltaDifference(sampleValue, referenceValue, mode);
    }
}

inline void ApplySourceDeltaReferenceValue(
    std::vector<MQuaternion> &samples,
    const MQuaternion &referenceValue,
    dcc_import_policy::SourceDeltaMode mode)
{
    for (MQuaternion &sampleValue : samples)
    {
        sampleValue = ApplySourceDeltaDifference(sampleValue, referenceValue, mode);
    }
}

inline void ApplySourceDeltaLinearReferenceSamples(
    std::vector<MVector> &samples,
    dcc_import_policy::SourceDeltaMode mode)
{
    if (samples.empty())
    {
        return;
    }

    const MVector firstValue = samples.front();
    const MVector lastValue = samples.back();
    const size_t sampleCount = samples.size();
    for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        double t = sampleCount > 1 ? static_cast<double>(sampleIndex) / static_cast<double>(sampleCount - 1) : 1.0;
        if (mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
        {
            t = ApplySplineWeight(t);
        }
        const MVector referenceValue = firstValue * (1.0 - t) + lastValue * t;
        samples[sampleIndex] = samples[sampleIndex] - referenceValue;
    }
}

inline void ApplySourceDeltaLinearReferenceSamples(
    std::vector<MQuaternion> &samples,
    dcc_import_policy::SourceDeltaMode mode)
{
    if (samples.empty())
    {
        return;
    }

    const MQuaternion firstValue = samples.front();
    const MQuaternion lastValue = samples.back();
    const size_t sampleCount = samples.size();
    for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        double t = sampleCount > 1 ? static_cast<double>(sampleIndex) / static_cast<double>(sampleCount - 1) : 1.0;
        if (mode == dcc_import_policy::SourceDeltaMode::SplineDelta)
        {
            t = ApplySplineWeight(t);
        }
        const MQuaternion referenceValue = slerp(firstValue, lastValue, t);
        samples[sampleIndex] = samples[sampleIndex] * referenceValue.inverse();
    }
}

} // namespace dcc_source_delta
