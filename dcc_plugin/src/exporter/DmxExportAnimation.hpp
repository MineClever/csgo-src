DmxElement *FindOrCreateFloatTargetElement(DmxTextBuilder &builder, ExportContext &context, const std::string &targetName)
{
    auto targetIt = context.floatTargetElementByName.find(targetName);
    if (targetIt != context.floatTargetElementByName.end())
    {
        return targetIt->second;
    }

    DmxElement *targetElement = builder.CreateElement("DmElement");
    targetElement->attributes.push_back(MakeScalarAttribute("name", "string", targetName));
    targetElement->attributes.push_back(MakeScalarAttribute("flexWeight", "float", "0.000000"));
    context.floatTargetElementByName[targetName] = targetElement;
    return targetElement;
}

DmxElement *BuildFloatLog(
    DmxTextBuilder &builder,
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<double> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatFloat(values[keyIndex]));
    }

    DmxElement *logLayer = builder.CreateElement("DmeFloatLogLayer");
    logLayer->attributes.push_back(MakeScalarAttribute("name", "string", "base"));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("times", "time_array", std::move(timeStrings)));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("values", "float_array", std::move(valueStrings)));

    DmxElement *logElement = builder.CreateElement("DmeFloatLog");
    logElement->attributes.push_back(MakeScalarAttribute("name", "string", logName));
    logElement->attributes.push_back(MakeElementArrayAttribute("layers", {logLayer}));
    return logElement;
}

DmxElement *BuildVector3Log(
    DmxTextBuilder &builder,
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<std::array<double, 3>> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatVector3(values[keyIndex][0], values[keyIndex][1], values[keyIndex][2]));
    }

    DmxElement *logLayer = builder.CreateElement("DmeVector3LogLayer");
    logLayer->attributes.push_back(MakeScalarAttribute("name", "string", "base"));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("times", "time_array", std::move(timeStrings)));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("values", "vector3_array", std::move(valueStrings)));

    DmxElement *logElement = builder.CreateElement("DmeVector3Log");
    logElement->attributes.push_back(MakeScalarAttribute("name", "string", logName));
    logElement->attributes.push_back(MakeElementArrayAttribute("layers", {logLayer}));
    return logElement;
}

DmxElement *BuildQuaternionLog(
    DmxTextBuilder &builder,
    const std::string &logName,
    const std::vector<double> &times,
    const std::vector<MQuaternion> &values)
{
    if (times.empty() || values.empty() || times.size() != values.size())
    {
        return nullptr;
    }

    std::vector<std::string> timeStrings;
    std::vector<std::string> valueStrings;
    timeStrings.reserve(times.size());
    valueStrings.reserve(values.size());
    for (size_t keyIndex = 0; keyIndex < times.size(); ++keyIndex)
    {
        timeStrings.push_back(FormatTimeSeconds(times[keyIndex]));
        valueStrings.push_back(FormatQuaternion(values[keyIndex].x, values[keyIndex].y, values[keyIndex].z, values[keyIndex].w));
    }

    DmxElement *logLayer = builder.CreateElement("DmeQuaternionLogLayer");
    logLayer->attributes.push_back(MakeScalarAttribute("name", "string", "base"));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("times", "time_array", std::move(timeStrings)));
    logLayer->attributes.push_back(MakeScalarArrayAttribute("values", "quaternion_array", std::move(valueStrings)));

    DmxElement *logElement = builder.CreateElement("DmeQuaternionLog");
    logElement->attributes.push_back(MakeScalarAttribute("name", "string", logName));
    logElement->attributes.push_back(MakeElementArrayAttribute("layers", {logLayer}));
    return logElement;
}

DmxElement *BuildFloatChannel(DmxTextBuilder &builder, const std::string &name, DmxElement *targetElement, const std::string &attributeName, DmxElement *logElement)
{
    if (!targetElement || !logElement)
    {
        return nullptr;
    }

    DmxElement *channelElement = builder.CreateElement("DmeChannel");
    channelElement->attributes.push_back(MakeScalarAttribute("name", "string", name));
    channelElement->attributes.push_back(MakeInlineElementAttribute("toElement", targetElement));
    channelElement->attributes.push_back(MakeScalarAttribute("toAttribute", "string", attributeName));
    channelElement->attributes.push_back(MakeInlineElementAttribute("log", logElement));
    return channelElement;
}

void AppendScalarAnimationChannel(
    DmxTextBuilder &builder,
    const MPlug &plug,
    DmxElement *targetElement,
    const std::string &attributeName,
    const std::string &channelName,
    std::vector<DmxElement *> &channels,
    double &clipDurationSeconds)
{
    if (plug.isNull() || !targetElement)
    {
        return;
    }

    const MObject curveObject = FindAnimationCurveForPlug(plug);
    if (curveObject.isNull())
    {
        return;
    }

    std::vector<double> times;
    AppendCurveTimes(curveObject, times);
    if (times.empty())
    {
        return;
    }

    std::vector<double> values;
    values.reserve(times.size());
    for (double timeSeconds : times)
    {
        values.push_back(EvaluateCurveOrValue(curveObject, plug, timeSeconds));
        clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
    }

    DmxElement *logElement = BuildFloatLog(builder, channelName + "_log", times, values);
    if (!logElement)
    {
        return;
    }

    if (DmxElement *channelElement = BuildFloatChannel(builder, channelName, targetElement, attributeName, logElement))
    {
        channels.push_back(channelElement);
    }
}

void AppendTransformAnimationChannels(
    DmxTextBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::vector<DmxElement *> &channels,
    double &clipDurationSeconds)
{
    const auto transformIt = context.transformElementByPath.find(DagPathKey(dagPath));
    if (transformIt == context.transformElementByPath.end() || !transformIt->second)
    {
        return;
    }

    MStatus status;
    MFnDependencyNode nodeFn(dagPath.node(), &status);
    if (!status)
    {
        return;
    }

    const auto findPlug = [&](const char *name) -> MPlug {
        return nodeFn.findPlug(name, true, &status);
    };

    MPlug txPlug = findPlug("translateX");
    MObject txCurve = FindAnimationCurveForPlug(txPlug);
    MPlug tyPlug = findPlug("translateY");
    MObject tyCurve = FindAnimationCurveForPlug(tyPlug);
    MPlug tzPlug = findPlug("translateZ");
    MObject tzCurve = FindAnimationCurveForPlug(tzPlug);
    std::vector<double> positionTimes;
    AppendCurveTimes(txCurve, positionTimes);
    AppendCurveTimes(tyCurve, positionTimes);
    AppendCurveTimes(tzCurve, positionTimes);
    if (!positionTimes.empty())
    {
        std::vector<std::array<double, 3>> positionValues;
        positionValues.reserve(positionTimes.size());
        for (double timeSeconds : positionTimes)
        {
            positionValues.push_back({
                EvaluateCurveOrValue(txCurve, txPlug, timeSeconds),
                EvaluateCurveOrValue(tyCurve, tyPlug, timeSeconds),
                EvaluateCurveOrValue(tzCurve, tzPlug, timeSeconds)});
            clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
        }

        DmxElement *logElement = BuildVector3Log(builder, std::string(dagPath.partialPathName().asChar()) + "_position", positionTimes, positionValues);
        if (logElement)
        {
            channels.push_back(BuildFloatChannel(builder, std::string(dagPath.partialPathName().asChar()) + "_position_channel", transformIt->second, "position", logElement));
        }
    }

    MPlug rxPlug = findPlug("rotateX");
    MObject rxCurve = FindAnimationCurveForPlug(rxPlug);
    MPlug ryPlug = findPlug("rotateY");
    MObject ryCurve = FindAnimationCurveForPlug(ryPlug);
    MPlug rzPlug = findPlug("rotateZ");
    MObject rzCurve = FindAnimationCurveForPlug(rzPlug);
    std::vector<double> rotationTimes;
    AppendCurveTimes(rxCurve, rotationTimes);
    AppendCurveTimes(ryCurve, rotationTimes);
    AppendCurveTimes(rzCurve, rotationTimes);
    if (!rotationTimes.empty())
    {
        std::vector<MQuaternion> rotationValues;
        rotationValues.reserve(rotationTimes.size());
        for (double timeSeconds : rotationTimes)
        {
            const double rx = EvaluateCurveOrValue(rxCurve, rxPlug, timeSeconds);
            const double ry = EvaluateCurveOrValue(ryCurve, ryPlug, timeSeconds);
            const double rz = EvaluateCurveOrValue(rzCurve, rzPlug, timeSeconds);
            rotationValues.push_back(MEulerRotation(rx, ry, rz).asQuaternion());
            clipDurationSeconds = std::max(clipDurationSeconds, timeSeconds);
        }

        DmxElement *logElement = BuildQuaternionLog(builder, std::string(dagPath.partialPathName().asChar()) + "_orientation", rotationTimes, rotationValues);
        if (logElement)
        {
            if (DmxElement *channelElement = BuildFloatChannel(builder, std::string(dagPath.partialPathName().asChar()) + "_orientation_channel", transformIt->second, "orientation", logElement))
            {
                channels.push_back(channelElement);
            }
        }
    }

    AppendScalarAnimationChannel(builder, findPlug("scaleX"), transformIt->second, "scaleX", std::string(dagPath.partialPathName().asChar()) + "_scaleX_channel", channels, clipDurationSeconds);
    AppendScalarAnimationChannel(builder, findPlug("scaleY"), transformIt->second, "scaleY", std::string(dagPath.partialPathName().asChar()) + "_scaleY_channel", channels, clipDurationSeconds);
    AppendScalarAnimationChannel(builder, findPlug("scaleZ"), transformIt->second, "scaleZ", std::string(dagPath.partialPathName().asChar()) + "_scaleZ_channel", channels, clipDurationSeconds);
}

void AppendControlAnimationChannels(
    DmxTextBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<DmxElement *> &channels,
    double &clipDurationSeconds)
{
    MStatus status;
    MFnDependencyNode nodeFn(dagPath.node(), &status);
    if (!status)
    {
        return;
    }

    const std::string nodeName = nodeFn.name().asChar();
    if (nodeName.size() < 9 || nodeName.rfind("_controls") != nodeName.size() - 9)
    {
        return;
    }

    MStringArray keyableAttributes;
    MString command = "listAttr -k -scalar \"";
    command += nodeFn.name();
    command += "\"";
    if (MGlobal::executeCommand(command, keyableAttributes, false, false) != MS::kSuccess)
    {
        return;
    }

    for (unsigned int attributeIndex = 0; attributeIndex < keyableAttributes.length(); ++attributeIndex)
    {
        const std::string attributeName = keyableAttributes[attributeIndex].asChar();
        if (attributeName == "translateX" || attributeName == "translateY" || attributeName == "translateZ" ||
            attributeName == "rotateX" || attributeName == "rotateY" || attributeName == "rotateZ" ||
            attributeName == "scaleX" || attributeName == "scaleY" || attributeName == "scaleZ" ||
            attributeName == "visibility")
        {
            continue;
        }

        MPlug plug = nodeFn.findPlug(attributeName.c_str(), true, &status);
        if (!status || plug.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        DmxElement *targetElement = FindOrCreateFloatTargetElement(builder, context, attributeName);
        const size_t beforeChannelCount = channels.size();
        AppendScalarAnimationChannel(builder, plug, targetElement, "flexWeight", attributeName + "_flex_channel", channels, clipDurationSeconds);
        if (channels.size() != beforeChannelCount)
        {
            exportedFlexTargets.insert(attributeName);
        }
    }
}

void AppendBlendShapeAnimationChannels(
    DmxTextBuilder &builder,
    const MDagPath &meshPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<DmxElement *> &channels,
    double &clipDurationSeconds)
{
    MStatus status;
    MObject meshObjectCopy(meshPath.node());
    MItDependencyGraph iterator(
        meshObjectCopy,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return;
    }

    for (; !iterator.isDone(); iterator.next())
    {
        MObject blendShapeObject = iterator.currentItem(&status);
        if (!status || blendShapeObject.isNull())
        {
            status = MS::kSuccess;
            continue;
        }

        MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MStringArray weightAliases;
        MString command = "listAttr -m \"";
        command += blendShapeNodeFn.name();
        command += ".w\"";
        if (MGlobal::executeCommand(command, weightAliases, false, false) != MS::kSuccess)
        {
            continue;
        }

        for (unsigned int aliasIndex = 0; aliasIndex < weightAliases.length(); ++aliasIndex)
        {
            const std::string aliasName = weightAliases[aliasIndex].asChar();
            if (exportedFlexTargets.find(aliasName) != exportedFlexTargets.end())
            {
                continue;
            }

            MPlug weightPlug = blendShapeNodeFn.findPlug(aliasName.c_str(), true, &status);
            if (!status || weightPlug.isNull())
            {
                status = MS::kSuccess;
                continue;
            }

            DmxElement *targetElement = FindOrCreateFloatTargetElement(builder, context, aliasName);
            const size_t beforeChannelCount = channels.size();
            AppendScalarAnimationChannel(builder, weightPlug, targetElement, "flexWeight", aliasName + "_flex_channel", channels, clipDurationSeconds);
            if (channels.size() != beforeChannelCount)
            {
                exportedFlexTargets.insert(aliasName);
            }
        }
    }
}

void AppendAnimationChannelsRecursive(
    DmxTextBuilder &builder,
    const MDagPath &dagPath,
    ExportContext &context,
    std::unordered_set<std::string> &exportedFlexTargets,
    std::vector<DmxElement *> &channels,
    double &clipDurationSeconds)
{
    if (!dagPath.isValid())
    {
        return;
    }

    AppendTransformAnimationChannels(builder, dagPath, context, channels, clipDurationSeconds);
    AppendControlAnimationChannels(builder, dagPath, context, exportedFlexTargets, channels, clipDurationSeconds);

    MStatus status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status)
    {
        return;
    }

    for (unsigned int childIndex = 0; childIndex < dagNode.childCount(); ++childIndex)
    {
        MObject childObject = dagNode.child(childIndex, &status);
        if (!status)
        {
            status = MS::kSuccess;
            continue;
        }

        if (childObject.hasFn(MFn::kMesh))
        {
            MDagPath meshPath = dagPath;
            meshPath.push(childObject);
            MFnDagNode meshDagNode(meshPath, &status);
            if (status && !meshDagNode.isIntermediateObject())
            {
                AppendBlendShapeAnimationChannels(builder, meshPath, context, exportedFlexTargets, channels, clipDurationSeconds);
            }
            status = MS::kSuccess;
            continue;
        }

        if (!(childObject.hasFn(MFn::kTransform) || childObject.hasFn(MFn::kJoint)))
        {
            continue;
        }

        MDagPath childPath = dagPath;
        childPath.push(childObject);
        AppendAnimationChannelsRecursive(builder, childPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }
}

DmxElement *BuildAnimationListElement(DmxTextBuilder &builder, const std::vector<MDagPath> &exportRoots, ExportContext &context)
{
    std::vector<DmxElement *> channels;
    std::unordered_set<std::string> exportedFlexTargets;
    double clipDurationSeconds = 0.0;

    for (const MDagPath &rootPath : exportRoots)
    {
        AppendAnimationChannelsRecursive(builder, rootPath, context, exportedFlexTargets, channels, clipDurationSeconds);
    }

    if (channels.empty())
    {
        return nullptr;
    }

    DmxElement *timeFrameElement = builder.CreateElement("DmeTimeFrame");
    timeFrameElement->attributes.push_back(MakeScalarAttribute("duration", "time", FormatTimeSeconds(clipDurationSeconds)));
    timeFrameElement->attributes.push_back(MakeScalarAttribute("frameRate", "float", "30.0"));

    DmxElement *clipElement = builder.CreateElement("DmeChannelsClip");
    clipElement->attributes.push_back(MakeScalarAttribute("name", "string", "maya_export_animation"));
    clipElement->attributes.push_back(MakeElementArrayAttribute("channels", channels));
    clipElement->attributes.push_back(MakeInlineElementAttribute("timeFrame", timeFrameElement));

    DmxElement *animationListElement = builder.CreateElement("DmeAnimationList");
    animationListElement->attributes.push_back(MakeScalarAttribute("name", "string", "animationList"));
    animationListElement->attributes.push_back(MakeElementArrayAttribute("animations", {clipElement}));
    return animationListElement;
}
