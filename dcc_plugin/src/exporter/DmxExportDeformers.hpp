void AppendSkinningData(const MDagPath &meshPath, DmxElement &vertexDataElement, ExportContext &context)
{
    MString skinClusterNodeName;
    const MString command = MString("findRelatedSkinCluster \"") + meshPath.fullPathName() + "\"";
    if (MGlobal::executeCommand(command, skinClusterNodeName) != MS::kSuccess || skinClusterNodeName.length() == 0)
    {
        return;
    }

    MSelectionList selectionList;
    if (selectionList.add(skinClusterNodeName) != MS::kSuccess)
    {
        return;
    }

    MObject skinClusterObject;
    if (selectionList.getDependNode(0, skinClusterObject) != MS::kSuccess)
    {
        return;
    }

    MStatus status;
    MFnSkinCluster skinClusterFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return;
    }

    MFnSingleIndexedComponent componentFn;
    MObject vertexComponent = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status)
    {
        return;
    }

    MIntArray vertexIds;
    const unsigned int vertexCount = meshFn.numVertices(&status);
    if (!status || vertexCount == 0)
    {
        return;
    }

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertexIds.append(static_cast<int>(vertexIndex));
    }

    status = componentFn.addElements(vertexIds);
    if (!status)
    {
        return;
    }

    MDagPathArray influencePaths;
    const unsigned int influenceCount = skinClusterFn.influenceObjects(influencePaths, &status);
    if (!status || influenceCount == 0)
    {
        return;
    }

    std::vector<int> influenceToJointIndex(influenceCount, -1);
    for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
    {
        const std::string pathKey = DagPathKey(influencePaths[influenceIndex]);
        auto it = context.jointIndexByPath.find(pathKey);
        if (it == context.jointIndexByPath.end())
        {
            auto dagIt = context.dagElementByPath.find(pathKey);
            if (dagIt != context.dagElementByPath.end() && dagIt->second)
            {
                const int jointIndex = static_cast<int>(context.jointElements.size());
                context.jointIndexByPath[pathKey] = jointIndex;
                context.jointElements.push_back(dagIt->second);
                it = context.jointIndexByPath.find(pathKey);
            }
        }
        if (it != context.jointIndexByPath.end())
        {
            influenceToJointIndex[influenceIndex] = it->second;
        }
    }

    MDoubleArray weights;
    unsigned int exportedInfluenceCount = 0;
    status = skinClusterFn.getWeights(meshPath, vertexComponent, weights, exportedInfluenceCount);
    if (!status || exportedInfluenceCount != influenceCount)
    {
        return;
    }

    constexpr double kWeightEpsilon = 1.0e-5;
    unsigned int jointCount = 0;
    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        unsigned int activeCount = 0;
        const unsigned int baseOffset = vertexIndex * influenceCount;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            if (influenceToJointIndex[influenceIndex] < 0)
            {
                continue;
            }

            if (std::abs(weights[baseOffset + influenceIndex]) > kWeightEpsilon)
            {
                ++activeCount;
            }
        }

        jointCount = std::max(jointCount, activeCount);
    }

    if (jointCount == 0)
    {
        return;
    }

    std::vector<std::string> jointWeightValues;
    std::vector<std::string> jointIndexValues;
    jointWeightValues.reserve(static_cast<size_t>(vertexCount) * jointCount);
    jointIndexValues.reserve(static_cast<size_t>(vertexCount) * jointCount);

    for (unsigned int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const unsigned int baseOffset = vertexIndex * influenceCount;
        std::vector<std::pair<int, double>> vertexWeights;
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            const int jointIndex = influenceToJointIndex[influenceIndex];
            const double weightValue = weights[baseOffset + influenceIndex];
            if (jointIndex < 0 || std::abs(weightValue) <= kWeightEpsilon)
            {
                continue;
            }

            vertexWeights.push_back({jointIndex, weightValue});
        }

        std::sort(vertexWeights.begin(), vertexWeights.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second != rhs.second)
            {
                return lhs.second > rhs.second;
            }
            return lhs.first < rhs.first;
        });

        if (vertexWeights.size() > jointCount)
        {
            vertexWeights.resize(jointCount);

            double weightSum = 0.0;
            for (const auto &entry : vertexWeights)
            {
                weightSum += entry.second;
            }

            if (weightSum > kWeightEpsilon)
            {
                for (auto &entry : vertexWeights)
                {
                    entry.second /= weightSum;
                }
            }
        }

        while (vertexWeights.size() < jointCount)
        {
            vertexWeights.push_back({0, 0.0});
        }

        for (const auto &entry : vertexWeights)
        {
            jointIndexValues.push_back(std::to_string(entry.first));
            jointWeightValues.push_back(FormatFloat(entry.second));
        }
    }

    vertexDataElement.attributes.push_back(MakeScalarAttribute("jointCount", "int", std::to_string(jointCount)));
    vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("jointIndices", "int_array", std::move(jointIndexValues)));
    vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("jointWeights", "float_array", std::move(jointWeightValues)));

    MFnDependencyNode skinClusterNodeFn(skinClusterObject, &status);
    if (!status)
    {
        return;
    }

    if (!context.exportMetadata)
    {
        return;
    }

    vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaDeformerType", "string", "skinCluster"));
    vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaSkinClusterName", "string", skinClusterNodeFn.name().asChar()));

    MPlug skinningMethodPlug = skinClusterNodeFn.findPlug("skinningMethod", true, &status);
    if (status)
    {
        short skinningMethod = 0;
        if (skinningMethodPlug.getValue(skinningMethod) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaSkinningMethod", "int", std::to_string(static_cast<int>(skinningMethod))));
        }
    }

    MPlug maxInfluencesPlug = skinClusterNodeFn.findPlug("maxInfluences", true, &status);
    if (status)
    {
        int maxInfluences = 0;
        if (maxInfluencesPlug.getValue(maxInfluences) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaMaxInfluences", "int", std::to_string(maxInfluences)));
        }
    }

    MPlug maintainMaxInfluencesPlug = skinClusterNodeFn.findPlug("maintainMaxInfluences", true, &status);
    if (status)
    {
        bool maintainMaxInfluences = false;
        if (maintainMaxInfluencesPlug.getValue(maintainMaxInfluences) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaMaintainMaxInfluences", "bool", maintainMaxInfluences ? "1" : "0"));
        }
    }

    MPlug normalizeWeightsPlug = skinClusterNodeFn.findPlug("normalizeWeights", true, &status);
    if (status)
    {
        short normalizeWeights = 0;
        if (normalizeWeightsPlug.getValue(normalizeWeights) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaNormalizeWeights", "int", std::to_string(static_cast<int>(normalizeWeights))));
        }
    }

    MPlug useComponentsPlug = skinClusterNodeFn.findPlug("useComponents", true, &status);
    if (status)
    {
        bool useComponents = false;
        if (useComponentsPlug.getValue(useComponents) == MS::kSuccess)
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaUseComponents", "bool", useComponents ? "1" : "0"));
        }
    }

    MPlug geomMatrixPlug = skinClusterNodeFn.findPlug("geomMatrix", true, &status);
    if (status)
    {
        const std::string geomMatrixValue = ReadMatrixPlugValue(geomMatrixPlug);
        if (!geomMatrixValue.empty())
        {
            vertexDataElement.attributes.push_back(MakeScalarAttribute("mayaGeomMatrix", "string", geomMatrixValue));
        }
    }

    std::vector<std::string> bindPreMatrixValues;
    std::vector<std::string> influencePathValues;
    bindPreMatrixValues.reserve(influenceCount);
    influencePathValues.reserve(influenceCount);
    MPlug bindPreMatrixArrayPlug = skinClusterNodeFn.findPlug("bindPreMatrix", true, &status);
    if (status)
    {
        for (unsigned int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
        {
            influencePathValues.push_back(influencePaths[influenceIndex].fullPathName().asChar());

            MPlug bindPreMatrixPlug = bindPreMatrixArrayPlug.elementByLogicalIndex(influenceIndex, &status);
            if (!status)
            {
                bindPreMatrixValues.push_back(FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()));
                status = MS::kSuccess;
                continue;
            }

            const std::string bindPreMatrixValue = ReadMatrixPlugValue(bindPreMatrixPlug);
            bindPreMatrixValues.push_back(
                bindPreMatrixValue.empty() ?
                FormatMatrix(influencePaths[influenceIndex].inclusiveMatrixInverse()) :
                bindPreMatrixValue);
        }
    }

    if (!influencePathValues.empty())
    {
        vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("mayaInfluencePaths", "string_array", std::move(influencePathValues)));
    }
    if (!bindPreMatrixValues.empty())
    {
        vertexDataElement.attributes.push_back(MakeScalarArrayAttribute("mayaBindPreMatrix", "string_array", std::move(bindPreMatrixValues)));
    }
}

void AppendBlendShapeDeltaStates(
    DmxTextBuilder &builder,
    const MDagPath &meshPath,
    const MPointArray &meshPoints,
    ExportContext &context,
    std::vector<DmxElement *> &deltaStateElements)
{
    MStatus status;
    MObject meshNodeObject = meshPath.node();
    MItDependencyGraph dependencyIt(
        meshNodeObject,
        MFn::kBlendShape,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);
    if (!status)
    {
        return;
    }

    for (; !dependencyIt.isDone(); dependencyIt.next())
    {
        MObject blendShapeObject = dependencyIt.currentItem(&status);
        if (!status || blendShapeObject.isNull())
        {
            continue;
        }

        MFnBlendShapeDeformer blendShapeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MIntArray weightIndices;
        status = blendShapeFn.weightIndexList(weightIndices);
        if (!status)
        {
            continue;
        }

        MFnDependencyNode blendShapeNodeFn(blendShapeObject, &status);
        if (!status)
        {
            continue;
        }

        MPlug weightArrayPlug = blendShapeNodeFn.findPlug("weight", true, &status);
        if (!status)
        {
            continue;
        }

        for (unsigned int weightSlot = 0; weightSlot < weightIndices.length(); ++weightSlot)
        {
            const unsigned int weightIndex = static_cast<unsigned int>(weightIndices[weightSlot]);
            MObjectArray targets;
            status = blendShapeFn.getTargets(meshNodeObject, static_cast<int>(weightIndex), targets);
            MDagPath targetPath;
            MString temporaryTargetTransform;
            MString targetNodeName;
            if (status && targets.length() > 0)
            {
                if (!TryGetMeshPathFromObject(targets[0], targetPath))
                {
                    continue;
                }

                MFnDagNode targetDagNode(targetPath, &status);
                if (!status)
                {
                    continue;
                }
                targetNodeName = targetDagNode.name();
            }
            else
            {
                if (!TryRegenerateBlendShapeTarget(blendShapeNodeFn.name(), weightIndex, targetPath, temporaryTargetTransform))
                {
                    continue;
                }

                MFnDagNode targetDagNode(targetPath, &status);
                if (!status)
                {
                    if (temporaryTargetTransform.length() > 0)
                    {
                        MString deleteCommand("delete \"");
                        deleteCommand += temporaryTargetTransform;
                        deleteCommand += "\"";
                        MGlobal::executeCommand(deleteCommand, false, false);
                    }
                    continue;
                }
                targetNodeName = targetDagNode.name();
            }

            MFnMesh targetMeshFn(targetPath, &status);
            if (!status)
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MString deleteCommand("delete \"");
                    deleteCommand += temporaryTargetTransform;
                    deleteCommand += "\"";
                    MGlobal::executeCommand(deleteCommand, false, false);
                }
                continue;
            }

            MPointArray targetPoints;
            status = targetMeshFn.getPoints(targetPoints, MSpace::kObject);
            if (!status || targetPoints.length() != meshPoints.length())
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MString deleteCommand("delete \"");
                    deleteCommand += temporaryTargetTransform;
                    deleteCommand += "\"";
                    MGlobal::executeCommand(deleteCommand, false, false);
                }
                continue;
            }

            std::vector<std::string> deltaPositions;
            std::vector<std::string> deltaPositionIndices;
            deltaPositions.reserve(targetPoints.length());
            deltaPositionIndices.reserve(targetPoints.length());
            for (unsigned int pointIndex = 0; pointIndex < targetPoints.length(); ++pointIndex)
            {
                const double dx = targetPoints[pointIndex].x - meshPoints[pointIndex].x;
                const double dy = targetPoints[pointIndex].y - meshPoints[pointIndex].y;
                const double dz = targetPoints[pointIndex].z - meshPoints[pointIndex].z;
                if (std::abs(dx) < 1.0e-6 && std::abs(dy) < 1.0e-6 && std::abs(dz) < 1.0e-6)
                {
                    continue;
                }

                deltaPositions.push_back(FormatVector3(dx, dy, dz));
                deltaPositionIndices.push_back(std::to_string(pointIndex));
            }

            if (deltaPositions.empty())
            {
                if (temporaryTargetTransform.length() > 0)
                {
                    MString deleteCommand("delete \"");
                    deleteCommand += temporaryTargetTransform;
                    deleteCommand += "\"";
                    MGlobal::executeCommand(deleteCommand, false, false);
                }
                continue;
            }

            std::string deltaName = targetNodeName.asChar();
            MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(weightIndex, &status);
            if (status)
            {
                MStringArray weightAliases;
                MString command = "listAttr -m \"";
                command += blendShapeNodeFn.name();
                command += ".w\"";
                if (MGlobal::executeCommand(command, weightAliases, false, false) == MS::kSuccess &&
                    weightSlot < weightAliases.length() &&
                    weightAliases[weightSlot].length() > 0)
                {
                    deltaName = weightAliases[weightSlot].asChar();
                }
            }

            DmxElement *deltaElement = builder.CreateElement("DmeVertexDeltaData");
            deltaElement->attributes.push_back(MakeScalarAttribute("name", "string", deltaName));
            deltaElement->attributes.push_back(MakeScalarArrayAttribute("vertexFormat", "string_array", {"positions"}));
            deltaElement->attributes.push_back(MakeScalarArrayAttribute("positions", "vector3_array", std::move(deltaPositions)));
            deltaElement->attributes.push_back(MakeScalarArrayAttribute("positionsIndices", "int_array", std::move(deltaPositionIndices)));
            if (context.exportMetadata)
            {
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaDeformerType", "string", "blendShape"));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeNode", "string", blendShapeNodeFn.name().asChar()));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaWeightIndex", "int", std::to_string(weightIndex)));
                deltaElement->attributes.push_back(MakeScalarAttribute("mayaTargetName", "string", targetNodeName.asChar()));
                MPlug envelopePlug = blendShapeNodeFn.findPlug("envelope", true, &status);
                if (status)
                {
                    float envelope = 1.0f;
                    if (envelopePlug.getValue(envelope) == MS::kSuccess)
                    {
                        deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeEnvelope", "float", FormatFloat(envelope)));
                    }
                }
                MPlug originPlug = blendShapeNodeFn.findPlug("origin", true, &status);
                if (status)
                {
                    short origin = 0;
                    if (originPlug.getValue(origin) == MS::kSuccess)
                    {
                        deltaElement->attributes.push_back(MakeScalarAttribute("mayaBlendShapeOrigin", "int", std::to_string(static_cast<int>(origin))));
                    }
                }
            }

            if (temporaryTargetTransform.length() > 0)
            {
                MString deleteCommand("delete \"");
                deleteCommand += temporaryTargetTransform;
                deleteCommand += "\"";
                MGlobal::executeCommand(deleteCommand, false, false);
            }

            deltaStateElements.push_back(deltaElement);
        }
    }
}
