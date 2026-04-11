#pragma once

#include "../common/SimpleDmxDocument.h"
#include "DmxExportTranslatorTypes.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MQuaternion.h>

namespace dmx_export_impl
{
using dmx_export_translator::ExportContext;

class AnimationExporter
{
public:
    AnimationExporter(
        std::shared_ptr<simple_dmx::DocumentBuilder> builder,
        std::shared_ptr<const std::vector<MDagPath>> exportRoots,
        std::shared_ptr<ExportContext> context);

    simple_dmx::Element *BuildAnimationListElement();

private:
    simple_dmx::Element *findOrCreateFloatTargetElement(const std::string &targetName);
    simple_dmx::Element *buildFloatLog(
        const std::string &logName,
        const std::vector<double> &times,
        const std::vector<double> &values);
    simple_dmx::Element *buildVector3Log(
        const std::string &logName,
        const std::vector<double> &times,
        const std::vector<std::array<double, 3>> &values);
    simple_dmx::Element *buildQuaternionLog(
        const std::string &logName,
        const std::vector<double> &times,
        const std::vector<MQuaternion> &values);
    simple_dmx::Element *buildFloatChannel(
        const std::string &name,
        simple_dmx::Element *targetElement,
        const std::string &attributeName,
        simple_dmx::Element *logElement);
    void appendScalarAnimationChannel(
        const MPlug &plug,
        simple_dmx::Element *targetElement,
        const std::string &attributeName,
        const std::string &channelName);
    void bindCurrentDagContext(const MDagPath &dagPath);
    void appendCurrentPositionAnimationChannels();
    void appendCurrentRotationAnimationChannels();
    void appendCurrentScaleAnimationChannels();
    void appendTransformAnimationChannels(const MDagPath &dagPath);
    void appendControlAnimationChannels(const MDagPath &dagPath);
    void appendBlendShapeAnimationChannels(const MDagPath &meshPath);
    void collectControlAnimationChannelsRecursive(const MDagPath &dagPath);
    void appendAnimationChannelsRecursive(const MDagPath &dagPath);

    std::shared_ptr<simple_dmx::DocumentBuilder> builder_;
    std::shared_ptr<const std::vector<MDagPath>> exportRoots_;
    std::shared_ptr<ExportContext> context_;
    std::unordered_set<std::string> exportedFlexTargets_;
    std::vector<simple_dmx::Element *> channels_;
    double clipDurationSeconds_ = 0.0;
    MDagPath currentDagPath_;
    simple_dmx::Element *currentTransformElement_ = nullptr;
    std::string currentDagName_;
};

simple_dmx::Element *BuildAnimationListElement(
    simple_dmx::DocumentBuilder &builder,
    const std::vector<MDagPath> &exportRoots,
    ExportContext &context);

} // namespace dmx_export_impl
