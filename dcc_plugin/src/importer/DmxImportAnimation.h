#pragma once

#include "DmxImportTranslatorTypes.h"

#include <common/SimpleDmxDocument.h>

#include <memory>
#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>

namespace dmx_import_impl
{
using dmx_import_translator::ImportContext;

class AnimationImporter
{
public:
    explicit AnimationImporter(std::shared_ptr<ImportContext> context);

    void setLookupRoots(
        const simple_dmx::Element *documentRoot,
        const simple_dmx::Element *importRoot,
        const simple_dmx::Element *modelRoot);

    const simple_dmx::Element *FindAnimationList() const;
    const simple_dmx::Element *FindCombinationOperator() const;

    MStatus ApplyChannelsClipAnimation(const simple_dmx::Element *channelsClip);
    MStatus CreateCombinationControls(
        const simple_dmx::Element *combinationOperator,
        const MObject &sceneRoot);

private:
    const simple_dmx::Element *findFirstLogLayer(const simple_dmx::Element *logElement) const;
    MStatus setCurveKeys(
        const MPlug &plug,
        const std::vector<double> &times,
        const std::vector<double> &values,
        MFnAnimCurve::AnimCurveType curveType) const;
    MStatus setCurveKeysAuto(
        const MPlug &plug,
        const std::vector<double> &times,
        const std::vector<double> &values) const;
    MStatus applyVector3Animation(const MDagPath &targetPath, const simple_dmx::Element *logLayer) const;
    MStatus applyQuaternionAnimation(const MDagPath &targetPath, const simple_dmx::Element *logLayer) const;
    MStatus addScalarAnimationTarget(std::vector<MPlug> &targets, const MObject &nodeObject, const std::string &attributeName) const;
    MStatus ensureControlAttributeTargets(const std::string &targetName);
    MStatus collectFloatAnimationTargets(
        const simple_dmx::Element *targetElement,
        const std::string &attributeName,
        std::vector<MPlug> &targets);
    MStatus applyFloatAnimation(const MPlug &targetPlug, const simple_dmx::Element *logLayer) const;
    MStatus applyFloatAnimation(const std::vector<MPlug> &targetPlugs, const simple_dmx::Element *logLayer) const;
    bool shouldSkipAppendTransformAnimation(const simple_dmx::Element *targetElement) const;
    bool shouldSkipAppendScalarAnimation(const MPlug &targetPlug) const;
    MObject findExistingControlNode(const std::string &controlNodeName, const MObject &sceneRoot) const;
    MStatus registerImportedControlPath(const MObject &controlNodeObject);
    void registerScalarTargetBinding(
        const std::string &targetName,
        const dmx_import_translator::ScalarAttributeBinding &binding);
    void bindCurrentChannel(const simple_dmx::Element *channel);

    std::shared_ptr<ImportContext> context_;
    const simple_dmx::Element *documentRoot_ = nullptr;
    const simple_dmx::Element *importRoot_ = nullptr;
    const simple_dmx::Element *modelRoot_ = nullptr;
    const simple_dmx::Element *currentChannel_ = nullptr;
    const simple_dmx::Element *currentTargetElement_ = nullptr;
    const simple_dmx::Element *currentLogElement_ = nullptr;
    const simple_dmx::Element *currentLogLayer_ = nullptr;
    std::string currentTargetAttribute_;
};

const simple_dmx::Element *FindAnimationList(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot);

const simple_dmx::Element *FindCombinationOperator(
    const simple_dmx::Document &document,
    const simple_dmx::Element *documentRoot,
    const simple_dmx::Element *importRoot,
    const simple_dmx::Element *modelRoot);

MStatus ApplyChannelsClipAnimation(ImportContext &context, const simple_dmx::Element *channelsClip);
MStatus CreateCombinationControls(
    ImportContext &context,
    const simple_dmx::Element *combinationOperator,
    const MObject &sceneRoot);

} // namespace dmx_import_impl
