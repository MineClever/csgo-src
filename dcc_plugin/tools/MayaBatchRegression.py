import argparse
import os
import sys

import maya.standalone

FORMAT_CONFIGS = {
    "dmx": {
        "extensions": (".dmx", ".dmxb", ".dmxbin"),
        "import_type": "Valve DMX Import",
        "export_type": "Valve DMX Export",
        "export_variants": [
            {
                "name": "text",
                "extension": ".dmx",
                "options": "",
                "mesh_marker_suffix": "roundtrip_text_meshcheck.txt",
                "type_marker_suffix": "roundtrip_text_typecheck.txt",
                "skin_marker_suffix": "roundtrip_text_skincheck.txt",
                "blendshape_marker_suffix": "roundtrip_text_blendshapecheck.txt",
                "animation_marker_suffix": "roundtrip_text_animcheck.txt",
            },
            {
                "name": "binary",
                "extension": ".dmxb",
                "options": "binary=1",
                "mesh_marker_suffix": "roundtrip_binary_meshcheck.txt",
                "type_marker_suffix": "roundtrip_binary_typecheck.txt",
                "skin_marker_suffix": "roundtrip_binary_skincheck.txt",
                "blendshape_marker_suffix": "roundtrip_binary_blendshapecheck.txt",
                "animation_marker_suffix": "roundtrip_binary_animcheck.txt",
            },
        ],
    },
    "smd": {
        "extensions": (".smd",),
        "import_type": "Valve SMD Import",
        "export_type": "Valve SMD Export",
        "export_variants": [
            {
                "name": "text",
                "extension": ".smd",
                "options": "",
                "mesh_marker_suffix": "roundtrip_text_meshcheck.txt",
                "type_marker_suffix": "roundtrip_text_typecheck.txt",
                "skin_marker_suffix": "roundtrip_text_skincheck.txt",
                "blendshape_marker_suffix": "roundtrip_text_blendshapecheck.txt",
                "animation_marker_suffix": "roundtrip_text_animcheck.txt",
            },
        ],
    },
}

ANIMATION_GATE_EXPECTATIONS = {
    "MostComplexSampleSet/vcaanim_VertexAnim": {
        "min_animated_plugs": 2,
        "required_any_suffix_groups": [
            [".translateX", ".translateY", ".translateZ"],
            [".rotateX", ".rotateY", ".rotateZ"],
        ],
    },
    "simple_float_animation": {
        "min_animated_plugs": 1,
        "required_any_suffix_groups": [
            [".scaleX"],
        ],
    },
    "simple_blendshape_animation": {
        "min_animated_plugs": 2,
        "required_substrings": [
            "combinationOperator_controls.smile",
            "_blendShape.smile",
        ],
    },
    "MostComplexSampleSet/vcaanim_VertexAnim.smd": {
        "min_animated_plugs": 1,
        "required_any_suffix_groups": [
            [".translateX", ".translateY", ".translateZ"],
        ],
    },
    "ctm_fbi/ctm_fbi_anims/rom_skin.smd": {
        "min_animated_plugs": 1,
        "required_any_suffix_groups": [
            [".translateX", ".translateY", ".translateZ"],
            [".rotateX", ".rotateY", ".rotateZ"],
        ],
    },
    "ctm_fbi/ctm_fbi_anims/shield_deploy.smd": {
        "min_animated_plugs": 1,
        "required_any_suffix_groups": [
            [".translateX", ".translateY", ".translateZ"],
            [".rotateX", ".rotateY", ".rotateZ"],
        ],
    },
}

APPEND_GATE_EXPECTATIONS = {
    "simple_blendshape_animation": {
        "import_options": "useSceneRoot=1;importMode=append",
        "retain_values": [
            {"plug_suffix": "combinationOperator_controls.smile", "value": 0.75},
        ],
        "single_nodes": [
            {"pattern": "*combinationOperator_controls", "type": "transform"},
        ],
        "single_node_types": [
            {"type": "blendShape", "name_suffix": "blendshapeAnimMeshShape_blendShape"},
        ],
    },
    "MostComplexSampleSet/chr_mesh.smd": {
        "import_options": "useSceneRoot=1;importMode=append",
        "single_nodes": [
            {"pattern": "*pelvis", "type": "joint"},
            {"pattern": "*tex_d_bmp_grp1", "type": "transform"},
        ],
    },
    "ctm_fbi/ctm_fbi.smd": {
        "import_options": "useSceneRoot=1;importMode=append",
        "single_nodes": [
            {"pattern": "*ctm_fbi_pelvis", "type": "joint"},
        ],
    },
}

UPDATE_GATE_EXPECTATIONS = {
    "simple_blendshape_animation": {
        "import_options": "useSceneRoot=1;importMode=update",
        "overwrite_values": [
            {"plug_suffix": "combinationOperator_controls.smile", "value": 0.75},
        ],
        "single_nodes": [
            {"pattern": "*combinationOperator_controls", "type": "transform"},
        ],
        "single_node_types": [
            {"type": "blendShape", "name_suffix": "blendshapeAnimMeshShape_blendShape"},
        ],
    },
    "MostComplexSampleSet/chr_mesh.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
        "overwrite_values": [
            {"plug_suffix": "pelvis.translateX", "value": 17.0},
        ],
        "single_nodes": [
            {"pattern": "*pelvis", "type": "joint"},
            {"pattern": "*tex_d_bmp_grp1", "type": "transform"},
        ],
    },
    "ctm_fbi/ctm_fbi.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
        "single_nodes": [
            {"pattern": "*ctm_fbi_pelvis", "type": "joint"},
        ],
    },
}

TOPOLOGY_UPDATE_GATE_EXPECTATIONS = {
    "simple_mesh": {
        "import_options": "useSceneRoot=1;importMode=update",
        "warning_substrings": [
            "update skipped mesh overwrite because existing mesh topology did not match incoming mesh",
            "mismatch",
        ],
    },
}

PAIRED_UPDATE_GATE_EXPECTATIONS = {
    "Ellis/DMX/animation/c1m1_intro_mechanic.dmx": {
        "base_case": "Ellis/DMX/mechanic_model.dmx",
        "base_import_options": "useSceneRoot=1;importMode=create",
        "update_import_options": "useSceneRoot=1;importMode=update",
        "required_animated_plugs": [
            "|ValveBiped_Bip01_Pelvis.translateX",
            "|ValveBiped_Bip01_Pelvis.rotateX",
        ],
    },
}

SKIN_INFLUENCE_UPDATE_GATE_EXPECTATIONS = {
    "complex_chr_mesh": {
        "import_options": "useSceneRoot=1;importMode=update",
    },
    "MostComplexSampleSet/chr_mesh.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
    },
    "ctm_fbi/ctm_fbi.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
    },
}

# Verifies that skinCluster nodes are reused in-place (same node names) when
# the same file is imported twice with importMode=update, rather than being
# deleted and recreated.
SKIN_CLUSTER_REUSE_GATE_EXPECTATIONS = {
    "MostComplexSampleSet/chr_mesh.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
    },
    "ctm_fbi/ctm_fbi.smd": {
        "import_options": "useSceneRoot=1;importMode=update",
    },
}

ANIMATION_LAYER_IMPORT_GATE_EXPECTATIONS = {
    "MostComplexSampleSet/vcaanim_VertexAnim": {
        "base_case": "MostComplexSampleSet/vcaanim_VertexAnim.dmx",
        "base_import_options": "useSceneRoot=0;importMode=create",
        "update_import_options": "useSceneRoot=0;importMode=animationOnly;importAnimationToLayer=1;animationLayerMode=replace",
        "layer_name": "vcaanim_VertexAnim_dmx_layer",
        "base_plug": "|vca_arm|pelvis.translateX",
        "min_curve_count": 6,
    },
    "ctm_fbi/ctm_fbi_anims/rom_skin.smd": {
        "base_case": "ctm_fbi/ctm_fbi.smd",
        "base_import_options": "useSceneRoot=1;importMode=create",
        "update_import_options": "useSceneRoot=1;importMode=animationOnly;importAnimationToLayer=1;animationLayerMode=replace",
        "layer_name": "rom_skin_smd_layer",
        "base_plug": "|pelvis.translateX",
        "min_curve_count": 6,
    },
    "Ellis/DMX/animation/c1m1_intro_mechanic.dmx": {
        "base_case": "Ellis/DMX/mechanic_model.dmx",
        "base_import_options": "useSceneRoot=1;importMode=create",
        "update_import_options": "useSceneRoot=1;importMode=animationOnly;importAnimationToLayer=1;animationLayerMode=replace",
        "layer_name": "c1m1_intro_mechanic_dmx_layer",
        "base_plug": "|ValveBiped_Bip01_Pelvis.translateX",
        "min_curve_count": 6,
    },
    "ctm_fbi/ctm_fbi_anims/shield_deploy.smd": {
        "base_case": "ctm_fbi/ctm_fbi.smd",
        "base_import_options": "useSceneRoot=1;importMode=create",
        "update_import_options": "useSceneRoot=1;importMode=animationOnly;importAnimationToLayer=1;animationLayerMode=replace",
        "layer_name": "shield_deploy_smd_layer",
        "base_plug": "|pelvis.translateX",
        "min_curve_count": 6,
    },
}

SOURCE_DELTA_IMPORT_GATE_EXPECTATIONS = {
    "simple_source_delta_overlay": {
        "base_case": "simple_source_delta_base.dmx",
        "baseline_import_options": "useSceneRoot=1;importMode=create",
        "delta_import_options": "useSceneRoot=1;importMode=update;animationLayerMode=replace;sourceDeltaMode=subtract;sourceDeltaReferenceClip=base_pose;sourceDeltaTargetClip=overlay_pose;sourceDeltaReferenceFrame=0",
        "layer_name": "simple_source_delta_overlay_dmx_source_delta",
        "compare_plugs": [
            "|source_delta_joint.translateX",
            "|source_delta_joint.rotateZ",
        ],
        "sample_times": [0.0, 0.5, 1.0],
    },
}


def snapshot_scene_animation_only_counts(cmds):
    return {
        "joint_count": len(cmds.ls(type="joint", long=True) or []),
        "mesh_count": len(cmds.ls(type="mesh", long=True) or []),
        "blendshape_count": len(cmds.ls(type="blendShape", long=True) or []),
        "transform_count": len(cmds.ls(type="transform", long=True) or []),
    }


def snapshot_imported_node_types(root_paths):
    import maya.api.OpenMaya as om

    node_snapshots = {}
    selection = om.MSelectionList()
    for root_path in root_paths:
        selection.add(root_path)

    for index in range(selection.length()):
        root_dag_path = selection.getDagPath(index)
        root_prefix = root_dag_path.fullPathName()

        iterator = om.MItDag()
        iterator.reset(root_dag_path, om.MItDag.kDepthFirst, om.MFn.kInvalid)
        while not iterator.isDone():
            dag_path = iterator.getPath()
            if dag_path.hasFn(om.MFn.kTransform):
                full_path = dag_path.fullPathName()
                relative_path = full_path[len(root_prefix):].lstrip("|")
                node_key = "<root>" if not relative_path else relative_path
                node_snapshots[node_key] = "joint" if dag_path.hasFn(om.MFn.kJoint) else "transform"
            iterator.next()

    return node_snapshots


def snapshot_skin_bindings(cmds, root_paths):
    skin_snapshots = {}
    for root_path in root_paths:
        descendant_meshes = cmds.listRelatives(root_path, allDescendents=True, fullPath=True, type="mesh") or []
        root_meshes = cmds.listRelatives(root_path, shapes=True, fullPath=True, type="mesh") or []
        for mesh in root_meshes + descendant_meshes:
            if cmds.getAttr(mesh + ".intermediateObject"):
                continue

            parents = cmds.listRelatives(mesh, parent=True, fullPath=True) or []
            if not parents:
                continue

            mesh_parent = parents[0]
            relative_parent = mesh_parent[len(root_path):].lstrip("|")
            mesh_key = "<root>" if not relative_parent else relative_parent
            history = cmds.listHistory(mesh) or []
            skin_clusters = [node for node in history if cmds.nodeType(node) == "skinCluster"]
            if not skin_clusters:
                continue

            influences = cmds.skinCluster(skin_clusters[0], query=True, influence=True) or []
            skin_snapshots[mesh_key] = {
                "skin_cluster": skin_clusters[0],
                "influence_count": len(influences),
                "influences": sorted(influences),
            }

    return skin_snapshots


def snapshot_blendshape_bindings(cmds, root_paths):
    blendshape_snapshots = {}
    for root_path in root_paths:
        descendant_meshes = cmds.listRelatives(root_path, allDescendents=True, fullPath=True, type="mesh") or []
        root_meshes = cmds.listRelatives(root_path, shapes=True, fullPath=True, type="mesh") or []
        for mesh in root_meshes + descendant_meshes:
            if cmds.getAttr(mesh + ".intermediateObject"):
                continue

            parents = cmds.listRelatives(mesh, parent=True, fullPath=True) or []
            if not parents:
                continue

            mesh_parent = parents[0]
            relative_parent = mesh_parent[len(root_path):].lstrip("|")
            mesh_key = "<root>" if not relative_parent else relative_parent
            history = cmds.listHistory(mesh) or []
            blendshape_nodes = [node for node in history if cmds.nodeType(node) == "blendShape"]
            if not blendshape_nodes:
                continue

            weight_aliases = cmds.listAttr(blendshape_nodes[0] + ".w", multi=True) or []
            blendshape_snapshots[mesh_key] = {
                "blendshape": blendshape_nodes[0],
                "target_count": len(weight_aliases),
            }

    return blendshape_snapshots


def snapshot_animation_bindings(cmds, root_paths):
    animation_snapshots = {}

    def record_animated_plug(node_name, node_key, attribute_name):
        plug_name = f"{node_name}.{attribute_name}"
        source_connections = cmds.listConnections(plug_name, source=True, destination=False, plugs=True) or []
        key_count = cmds.keyframe(node_name, attribute=attribute_name, query=True, keyframeCount=True) or 0
        if not source_connections and not key_count:
            return

        animation_snapshots[f"{node_key}.{attribute_name}"] = {
            "key_count": int(key_count),
            "sources": sorted(source_connections),
        }

    for root_path in root_paths:
        descendant_transforms = cmds.listRelatives(root_path, allDescendents=True, fullPath=True, type="transform") or []
        root_transforms = [root_path]
        for node_name in root_transforms + descendant_transforms:
            relative_path = node_name[len(root_path):].lstrip("|")
            node_key = "<root>" if not relative_path else relative_path
            for attribute_name in cmds.listAttr(node_name, keyable=True, scalar=True) or []:
                record_animated_plug(node_name, node_key, attribute_name)

        descendant_meshes = cmds.listRelatives(root_path, allDescendents=True, fullPath=True, type="mesh") or []
        root_meshes = cmds.listRelatives(root_path, shapes=True, fullPath=True, type="mesh") or []
        for mesh in root_meshes + descendant_meshes:
            if cmds.getAttr(mesh + ".intermediateObject"):
                continue

            history = cmds.listHistory(mesh) or []
            blendshape_nodes = [node for node in history if cmds.nodeType(node) == "blendShape"]
            for blendshape_node in blendshape_nodes:
                for attribute_name in cmds.listAttr(blendshape_node + ".w", multi=True) or []:
                    record_animated_plug(blendshape_node, blendshape_node, attribute_name)

    return animation_snapshots


def snapshot_scene_meshes():
    import maya.api.OpenMaya as om

    mesh_snapshots = {}
    iterator = om.MItDag(om.MItDag.kDepthFirst, om.MFn.kMesh)
    while not iterator.isDone():
        dag_path = iterator.getPath()
        mesh_fn = om.MFnMesh(dag_path)
        if mesh_fn.isIntermediateObject:
            iterator.next()
            continue

        mesh_key = dag_path.fullPathName().split("|")[-1]
        points = mesh_fn.getPoints(om.MSpace.kObject)
        polygon_counts, polygon_connects = mesh_fn.getVertices()
        mesh_snapshots[mesh_key] = {
            "points": [(point.x, point.y, point.z) for point in points],
            "counts": list(polygon_counts),
            "connects": list(polygon_connects),
        }
        if not mesh_snapshots[mesh_key]["points"] or not mesh_snapshots[mesh_key]["counts"] or not mesh_snapshots[mesh_key]["connects"]:
            del mesh_snapshots[mesh_key]
        iterator.next()

    return mesh_snapshots


def find_single_visible_mesh_path(cmds, root_paths):
    mesh_paths = []
    for root_path in root_paths:
        descendant_meshes = cmds.listRelatives(root_path, allDescendents=True, fullPath=True, type="mesh") or []
        root_meshes = cmds.listRelatives(root_path, shapes=True, fullPath=True, type="mesh") or []
        for mesh_path in root_meshes + descendant_meshes:
            if cmds.getAttr(mesh_path + ".intermediateObject"):
                continue
            mesh_paths.append(mesh_path)

    unique_mesh_paths = sorted(set(mesh_paths))
    if len(unique_mesh_paths) != 1:
        raise RuntimeError(f"Expected exactly one visible mesh, got {unique_mesh_paths}")

    return unique_mesh_paths[0]


def capture_warning_messages(callback):
    import maya.OpenMaya as om

    warning_messages = []

    def command_output_callback(message, message_type, _client_data):
        if message_type == om.MCommandMessage.kWarning:
            warning_messages.append(message)

    callback_id = om.MCommandMessage.addCommandOutputCallback(command_output_callback)
    try:
        callback()
    finally:
        om.MMessage.removeCallback(callback_id)

    return warning_messages


def compare_mesh_snapshots(reference_meshes, candidate_meshes, tolerance=1.0e-4):
    if set(reference_meshes.keys()) != set(candidate_meshes.keys()):
        missing = sorted(set(reference_meshes.keys()) - set(candidate_meshes.keys()))
        extra = sorted(set(candidate_meshes.keys()) - set(reference_meshes.keys()))
        raise RuntimeError(f"Mesh set mismatch. Missing={missing} Extra={extra}")

    for mesh_name in sorted(reference_meshes.keys()):
        reference = reference_meshes[mesh_name]
        candidate = candidate_meshes[mesh_name]

        if reference["counts"] != candidate["counts"] or reference["connects"] != candidate["connects"]:
            first_count_diff = next(
                (
                    index,
                    reference["counts"][index],
                    candidate["counts"][index],
                )
                for index in range(min(len(reference["counts"]), len(candidate["counts"])))
                if reference["counts"][index] != candidate["counts"][index]
            ) if reference["counts"] != candidate["counts"] else None
            first_connect_diff = next(
                (
                    index,
                    reference["connects"][index],
                    candidate["connects"][index],
                )
                for index in range(min(len(reference["connects"]), len(candidate["connects"])))
                if reference["connects"][index] != candidate["connects"][index]
            ) if reference["connects"] != candidate["connects"] else None
            raise RuntimeError(
                f"Mesh topology mismatch for {mesh_name}. "
                f"counts=({len(reference['counts'])}->{len(candidate['counts'])}, first_diff={first_count_diff}) "
                f"connects=({len(reference['connects'])}->{len(candidate['connects'])}, first_diff={first_connect_diff})"
            )

        if len(reference["points"]) != len(candidate["points"]):
            raise RuntimeError(f"Mesh vertex count mismatch for {mesh_name}")

        for index, (lhs_point, rhs_point) in enumerate(zip(reference["points"], candidate["points"])):
            if (
                abs(lhs_point[0] - rhs_point[0]) > tolerance
                or abs(lhs_point[1] - rhs_point[1]) > tolerance
                or abs(lhs_point[2] - rhs_point[2]) > tolerance
            ):
                raise RuntimeError(f"Mesh point mismatch for {mesh_name} at vertex {index}")


def compare_node_type_snapshots(reference_nodes, candidate_nodes):
    def strip_single_wrapper(nodes):
        wrapper_names = set()
        stripped = {}
        for node_name, node_type in nodes.items():
            if node_name == "<root>":
                stripped[node_name] = node_type
                continue

            parts = node_name.split("|")
            wrapper_names.add(parts[0])
            stripped_name = "|".join(parts[1:]) if len(parts) > 1 else ""
            if stripped_name:
                stripped[stripped_name] = node_type
        if len(wrapper_names) != 1 or not stripped:
            return None
        return stripped

    def compare_exact(lhs_nodes, rhs_nodes):
        if set(lhs_nodes.keys()) != set(rhs_nodes.keys()):
            missing = sorted(set(lhs_nodes.keys()) - set(rhs_nodes.keys()))
            extra = sorted(set(rhs_nodes.keys()) - set(lhs_nodes.keys()))
            raise RuntimeError(f"Node set mismatch. Missing={missing} Extra={extra}")

        for node_name in sorted(lhs_nodes.keys()):
            if lhs_nodes[node_name] != rhs_nodes[node_name]:
                raise RuntimeError(
                    f"Node type mismatch for {node_name}. "
                    f"expected={lhs_nodes[node_name]} actual={rhs_nodes[node_name]}"
                )

    candidate_variants = [candidate_nodes]
    stripped_candidate = strip_single_wrapper(candidate_nodes)
    if stripped_candidate:
        candidate_variants.append(stripped_candidate)

    reference_variants = [reference_nodes]
    stripped_reference = strip_single_wrapper(reference_nodes)
    if stripped_reference:
        reference_variants.append(stripped_reference)

    last_error = None
    for reference_variant in reference_variants:
        for candidate_variant in candidate_variants:
            try:
                compare_exact(reference_variant, candidate_variant)
                return
            except RuntimeError as exc:
                last_error = exc

    raise last_error


def strip_single_wrapper_snapshot(nodes):
    wrapper_names = set()
    stripped = {}
    for node_name, node_data in nodes.items():
        if node_name == "<root>":
            stripped[node_name] = node_data
            continue

        parts = node_name.split("|")
        wrapper_names.add(parts[0])
        stripped_name = "|".join(parts[1:]) if len(parts) > 1 else ""
        if stripped_name:
            stripped[stripped_name] = node_data
    if len(wrapper_names) != 1 or not stripped:
        return None
    return stripped


def compare_skin_snapshots(reference_skins, candidate_skins):
    def compare_exact(lhs_skins, rhs_skins):
        if set(lhs_skins.keys()) != set(rhs_skins.keys()):
            missing = sorted(set(lhs_skins.keys()) - set(rhs_skins.keys()))
            extra = sorted(set(rhs_skins.keys()) - set(lhs_skins.keys()))
            raise RuntimeError(f"Skin set mismatch. Missing={missing} Extra={extra}")

        for mesh_key in sorted(lhs_skins.keys()):
            if lhs_skins[mesh_key]["influence_count"] != rhs_skins[mesh_key]["influence_count"]:
                raise RuntimeError(
                    f"Skin influence count mismatch for {mesh_key}. "
                    f"expected={lhs_skins[mesh_key]['influence_count']} actual={rhs_skins[mesh_key]['influence_count']}"
                )

            if lhs_skins[mesh_key].get("influences") != rhs_skins[mesh_key].get("influences"):
                raise RuntimeError(
                    f"Skin influence set mismatch for {mesh_key}. "
                    f"expected={lhs_skins[mesh_key].get('influences')} actual={rhs_skins[mesh_key].get('influences')}"
                )

    candidate_variants = [candidate_skins]
    stripped_candidate = strip_single_wrapper_snapshot(candidate_skins)
    if stripped_candidate:
        candidate_variants.append(stripped_candidate)

    reference_variants = [reference_skins]
    stripped_reference = strip_single_wrapper_snapshot(reference_skins)
    if stripped_reference:
        reference_variants.append(stripped_reference)

    last_error = None
    for reference_variant in reference_variants:
        for candidate_variant in candidate_variants:
            try:
                compare_exact(reference_variant, candidate_variant)
                return
            except RuntimeError as exc:
                last_error = exc

    raise last_error


def compare_blendshape_snapshots(reference_blendshapes, candidate_blendshapes):
    def compare_exact(lhs_blendshapes, rhs_blendshapes):
        if set(lhs_blendshapes.keys()) != set(rhs_blendshapes.keys()):
            missing = sorted(set(lhs_blendshapes.keys()) - set(rhs_blendshapes.keys()))
            extra = sorted(set(rhs_blendshapes.keys()) - set(lhs_blendshapes.keys()))
            raise RuntimeError(f"BlendShape set mismatch. Missing={missing} Extra={extra}")

    candidate_variants = [candidate_blendshapes]
    stripped_candidate = strip_single_wrapper_snapshot(candidate_blendshapes)
    if stripped_candidate:
        candidate_variants.append(stripped_candidate)

    reference_variants = [reference_blendshapes]
    stripped_reference = strip_single_wrapper_snapshot(reference_blendshapes)
    if stripped_reference:
        reference_variants.append(stripped_reference)

    last_error = None
    for reference_variant in reference_variants:
        for candidate_variant in candidate_variants:
            try:
                compare_exact(reference_variant, candidate_variant)
                return
            except RuntimeError as exc:
                last_error = exc

    raise last_error


def compare_animation_snapshots(reference_animations, candidate_animations):
    def strip_single_wrapper(nodes):
        wrapper_names = set()
        stripped = {}
        for node_name, node_data in nodes.items():
            split_index = node_name.rfind(".")
            path_name = node_name[:split_index]
            attribute_name = node_name[split_index + 1:]
            if path_name == "<root>":
                stripped[node_name] = node_data
                continue

            parts = path_name.split("|")
            if len(parts) == 1:
                # No path separator — non-DAG nodes (e.g. blendShape) are keyed by
                # their short name directly.  They have no wrapper to strip, so pass
                # them through unchanged without contributing to wrapper detection.
                stripped[node_name] = node_data
                continue

            wrapper_names.add(parts[0])
            stripped_path = "|".join(parts[1:])
            stripped[f"{stripped_path}.{attribute_name}"] = node_data
        if len(wrapper_names) != 1 or not stripped:
            return None
        return stripped

    def compare_exact(lhs_animations, rhs_animations):
        if set(lhs_animations.keys()) != set(rhs_animations.keys()):
            missing = sorted(set(lhs_animations.keys()) - set(rhs_animations.keys()))
            extra = sorted(set(rhs_animations.keys()) - set(lhs_animations.keys()))
            raise RuntimeError(f"Animation set mismatch. Missing={missing} Extra={extra}")

        for animation_key in sorted(lhs_animations.keys()):
            reference = lhs_animations[animation_key]
            candidate = rhs_animations[animation_key]
            if reference["key_count"] != candidate["key_count"]:
                raise RuntimeError(
                    f"Animation key count mismatch for {animation_key}. "
                    f"expected={reference['key_count']} actual={candidate['key_count']}"
                )
            if reference["sources"] != candidate["sources"]:
                raise RuntimeError(
                    f"Animation source mismatch for {animation_key}. "
                    f"expected={reference['sources']} actual={candidate['sources']}"
                )

    candidate_variants = [candidate_animations]
    stripped_candidate = strip_single_wrapper(candidate_animations)
    if stripped_candidate:
        candidate_variants.append(stripped_candidate)

    reference_variants = [reference_animations]
    stripped_reference = strip_single_wrapper(reference_animations)
    if stripped_reference:
        reference_variants.append(stripped_reference)

    last_error = None
    for reference_variant in reference_variants:
        for candidate_variant in candidate_variants:
            try:
                compare_exact(reference_variant, candidate_variant)
                return
            except RuntimeError as exc:
                last_error = exc

    raise last_error


def validate_animation_gate(case_name, animation_snapshots):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = ANIMATION_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    if len(animation_snapshots) < expectation.get("min_animated_plugs", 0):
        raise RuntimeError(
            f"Animation gate failed for {normalized_case_name}. "
            f"expected at least {expectation['min_animated_plugs']} animated plugs, got {len(animation_snapshots)}"
        )

    animation_keys = sorted(animation_snapshots.keys())
    for suffix_group in expectation.get("required_any_suffix_groups", []):
        if not any(any(animation_key.endswith(suffix) for suffix in suffix_group) for animation_key in animation_keys):
            raise RuntimeError(
                f"Animation gate failed for {normalized_case_name}. "
                f"missing animated plug matching one of: {suffix_group}"
            )

    for required_substring in expectation.get("required_substrings", []):
        if not any(required_substring in animation_key for animation_key in animation_keys):
            raise RuntimeError(
                f"Animation gate failed for {normalized_case_name}. "
                f"missing animated plug containing: {required_substring}"
            )


def resolve_expected_plug(cmds, plug_expectation):
    explicit_plug = plug_expectation.get("plug")
    if explicit_plug:
        return explicit_plug if cmds.objExists(explicit_plug) else None

    plug_suffix = plug_expectation.get("plug_suffix")
    if not plug_suffix:
        return None

    if "." not in plug_suffix:
        raise RuntimeError(f"Invalid plug_suffix without attribute name: {plug_suffix}")

    node_suffix, attribute_name = plug_suffix.rsplit(".", 1)
    candidate_nodes = cmds.ls("*" + node_suffix, long=True) or []
    candidate_plugs = []
    for node_name in candidate_nodes:
        candidate_plug = node_name + "." + attribute_name
        if cmds.objExists(candidate_plug):
            candidate_plugs.append(candidate_plug)

    unique_candidates = sorted(set(candidate_plugs))
    if len(unique_candidates) != 1:
        return None

    return unique_candidates[0]


def find_single_node_type_matches(cmds, expectation):
    candidate_nodes = cmds.ls(type=expectation["type"]) or []

    exact_name = expectation.get("name")
    if exact_name:
        return [node_name for node_name in candidate_nodes if node_name == exact_name]

    name_suffix = expectation.get("name_suffix")
    if name_suffix:
        return [node_name for node_name in candidate_nodes if node_name.endswith(name_suffix)]

    return candidate_nodes


def validate_append_gate(cmds, plugin_paths, format_config, input_path, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = APPEND_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        options=expectation["import_options"],
    )

    cmds.file(input_path, **import_kwargs)
    for retain_value in expectation.get("retain_values", []):
        plug_name = resolve_expected_plug(cmds, retain_value)
        if not plug_name:
            raise RuntimeError(
                f"Append gate failed for {normalized_case_name}. "
                f"missing plug before second append: {retain_value.get('plug') or retain_value.get('plug_suffix')}"
            )
        retain_value["_resolved_plug"] = plug_name
        cmds.setAttr(plug_name, retain_value["value"])

    cmds.file(input_path, **import_kwargs)

    for single_node in expectation.get("single_nodes", []):
        matching_nodes = cmds.ls(single_node["pattern"], long=True, type=single_node["type"]) or []
        if len(matching_nodes) != 1:
            raise RuntimeError(
                f"Append gate failed for {normalized_case_name}. "
                f"expected exactly one {single_node['type']} matching {single_node['pattern']}, got {matching_nodes}"
            )

    for single_node_type in expectation.get("single_node_types", []):
        matching_nodes = find_single_node_type_matches(cmds, single_node_type)
        if len(matching_nodes) != 1:
            raise RuntimeError(
                f"Append gate failed for {normalized_case_name}. "
                f"expected exactly one {single_node_type['type']} matching {single_node_type.get('name') or single_node_type.get('name_suffix')}, got {matching_nodes}"
            )

    for retain_value in expectation.get("retain_values", []):
        plug_name = retain_value.get("_resolved_plug") or resolve_expected_plug(cmds, retain_value)
        if not plug_name:
            raise RuntimeError(
                f"Append gate failed for {normalized_case_name}. "
                f"missing preserved plug after second append: {retain_value.get('plug') or retain_value.get('plug_suffix')}"
            )
        current_value = cmds.getAttr(plug_name)
        if abs(float(current_value) - float(retain_value["value"])) > 1.0e-6:
            raise RuntimeError(
                f"Append gate failed for {normalized_case_name}. "
                f"expected preserved value {plug_name}={retain_value['value']}, got {current_value}"
            )


def validate_update_gate(cmds, plugin_paths, format_config, input_path, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = UPDATE_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        options=expectation["import_options"],
    )

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    if not imported_roots:
        raise RuntimeError(
            f"Update gate failed for {normalized_case_name}. "
            f"first import produced no DAG roots"
        )

    reference_meshes = snapshot_scene_meshes()
    reference_node_types = snapshot_imported_node_types(imported_roots)
    reference_skins = snapshot_skin_bindings(cmds, imported_roots)
    reference_blendshapes = snapshot_blendshape_bindings(cmds, imported_roots)
    original_values = {}
    for overwrite_value in expectation.get("overwrite_values", []):
        plug_name = resolve_expected_plug(cmds, overwrite_value)
        if not plug_name:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"missing plug before second update: {overwrite_value.get('plug') or overwrite_value.get('plug_suffix')}"
            )
        overwrite_value["_resolved_plug"] = plug_name
        original_values[plug_name] = cmds.getAttr(plug_name)
        cmds.setAttr(plug_name, overwrite_value["value"])

    cmds.file(input_path, **import_kwargs)

    current_imported_roots = collect_imported_roots(cmds, before_assemblies)
    if sorted(imported_roots) != current_imported_roots:
        raise RuntimeError(
            f"Update gate failed for {normalized_case_name}. "
            f"assembly roots changed after second update. expected={sorted(imported_roots)} actual={current_imported_roots}"
        )

    for single_node in expectation.get("single_nodes", []):
        matching_nodes = cmds.ls(single_node["pattern"], long=True, type=single_node["type"]) or []
        if len(matching_nodes) != 1:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"expected exactly one {single_node['type']} matching {single_node['pattern']}, got {matching_nodes}"
            )

    for single_node_type in expectation.get("single_node_types", []):
        matching_nodes = find_single_node_type_matches(cmds, single_node_type)
        if len(matching_nodes) != 1:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"expected exactly one {single_node_type['type']} matching {single_node_type.get('name') or single_node_type.get('name_suffix')}, got {matching_nodes}"
            )

    for overwrite_value in expectation.get("overwrite_values", []):
        plug_name = overwrite_value.get("_resolved_plug") or resolve_expected_plug(cmds, overwrite_value)
        if not plug_name:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"missing overwritten plug after second update: {overwrite_value.get('plug') or overwrite_value.get('plug_suffix')}"
            )
        current_value = cmds.getAttr(plug_name)
        if abs(float(current_value) - float(overwrite_value["value"])) <= 1.0e-6:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"expected {plug_name} to be overwritten away from {overwrite_value['value']}, got {current_value}"
            )

        original_value = original_values[plug_name]
        if abs(float(current_value) - float(original_value)) > 1.0e-6:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. "
                f"expected restored value {plug_name}={original_value}, got {current_value}"
            )

    compare_mesh_snapshots(reference_meshes, snapshot_scene_meshes())
    compare_node_type_snapshots(reference_node_types, snapshot_imported_node_types(imported_roots))
    compare_skin_snapshots(reference_skins, snapshot_skin_bindings(cmds, imported_roots))
    compare_blendshape_snapshots(reference_blendshapes, snapshot_blendshape_bindings(cmds, imported_roots))


def validate_topology_update_gate(cmds, plugin_paths, format_config, input_path, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = TOPOLOGY_UPDATE_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        options=expectation["import_options"],
    )

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    mesh_path = find_single_visible_mesh_path(cmds, imported_roots)
    reference_meshes = snapshot_scene_meshes()

    cmds.xform(f"{mesh_path}.vtx[0]", relative=True, objectSpace=True, translation=(1.0, 0.0, 0.0))
    warning_messages = capture_warning_messages(lambda: cmds.file(input_path, **import_kwargs))
    if any(expectation["warning_substrings"][0] in warning for warning in warning_messages):
        raise RuntimeError(
            f"Topology update gate failed for {normalized_case_name}. "
            f"same-topology update unexpectedly produced topology mismatch warning: {warning_messages}"
        )
    compare_mesh_snapshots(reference_meshes, snapshot_scene_meshes())

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)
    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    mesh_path = find_single_visible_mesh_path(cmds, imported_roots)

    cmds.polySubdivideFacet(f"{mesh_path}.f[0]", divisions=1, constructionHistory=False)
    mutated_meshes = snapshot_scene_meshes()
    warning_messages = capture_warning_messages(lambda: cmds.file(input_path, **import_kwargs))
    if not any(expectation["warning_substrings"][0] in warning and expectation["warning_substrings"][1] in warning for warning in warning_messages):
        raise RuntimeError(
            f"Topology update gate failed for {normalized_case_name}. "
            f"missing topology mismatch warning, got {warning_messages}"
        )
    compare_mesh_snapshots(mutated_meshes, snapshot_scene_meshes())


def validate_paired_update_gate(cmds, plugin_paths_by_format, sample_dir, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = PAIRED_UPDATE_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    input_path = resolve_input_path(sample_dir, case_name)
    format_name = detect_format(input_path)
    format_config = FORMAT_CONFIGS[format_name]
    plugin_path = plugin_paths_by_format.get(format_name)
    if not plugin_path:
        raise RuntimeError(f"Missing plugin for paired update gate format '{format_name}' while running case '{case_name}'")

    base_input_path = resolve_input_path(sample_dir, expectation["base_case"])

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, [plugin_path])

    base_import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["base_import_options"],
    )
    update_import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["update_import_options"],
    )

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(base_input_path, **base_import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    if not imported_roots:
        raise RuntimeError(
            f"Paired update gate failed for {normalized_case_name}. "
            f"base import produced no DAG roots"
        )

    roots_before_update = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **update_import_kwargs)
    new_roots_after_update = sorted(set(cmds.ls(assemblies=True, long=True) or []) - roots_before_update)
    if new_roots_after_update:
        raise RuntimeError(
            f"Paired update gate failed for {normalized_case_name}. "
            f"update import created unexpected new roots: {new_roots_after_update}"
        )

    for plug_name in expectation.get("required_animated_plugs", []):
        if not cmds.objExists(plug_name):
            raise RuntimeError(
                f"Paired update gate failed for {normalized_case_name}. "
                f"missing expected plug after update import: {plug_name}"
            )

        source_connections = cmds.listConnections(plug_name, source=True, destination=False, plugs=True) or []
        key_count = cmds.keyframe(plug_name, query=True, keyframeCount=True) or 0
        if not source_connections and not key_count:
            raise RuntimeError(
                f"Paired update gate failed for {normalized_case_name}. "
                f"plug {plug_name} did not receive animation"
            )


def validate_skin_influence_update_gate(cmds, plugin_paths, format_config, input_path, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = SKIN_INFLUENCE_UPDATE_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["import_options"],
    )

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    reference_skins = snapshot_skin_bindings(cmds, imported_roots)
    if not reference_skins:
        raise RuntimeError(
            f"Skin influence update gate failed for {normalized_case_name}. "
            f"initial import produced no skinCluster"
        )

    target_skin = next(iter(reference_skins.values()))
    if target_skin["influence_count"] <= 1:
        raise RuntimeError(
            f"Skin influence update gate failed for {normalized_case_name}. "
            f"need at least two influences, got {target_skin['influence_count']}"
        )

    removed_influence = target_skin["influences"][-1]
    cmds.skinCluster(target_skin["skin_cluster"], edit=True, removeInfluence=removed_influence)

    mutated_skins = snapshot_skin_bindings(cmds, imported_roots)
    mutated_target_skin = next(iter(mutated_skins.values()))
    if removed_influence in mutated_target_skin.get("influences", []):
        raise RuntimeError(
            f"Skin influence update gate failed for {normalized_case_name}. "
            f"failed to remove influence before update: {removed_influence}"
        )

    cmds.file(input_path, **import_kwargs)
    compare_skin_snapshots(reference_skins, snapshot_skin_bindings(cmds, imported_roots))


def validate_skin_cluster_reuse_gate(cmds, plugin_paths, format_config, input_path, case_name):
    """
    Verifies that skinCluster nodes are reused in-place (same node names, not deleted+recreated)
    when the same file is imported twice with importMode=update.
    """
    normalized_case_name = case_name.replace("\\", "/")
    expectation = SKIN_CLUSTER_REUSE_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["import_options"],
    )

    cmds.file(input_path, **import_kwargs)
    skin_nodes_before = set(cmds.ls(type="skinCluster") or [])
    if not skin_nodes_before:
        raise RuntimeError(
            f"Skin cluster reuse gate failed for {normalized_case_name}. "
            f"initial import produced no skinCluster nodes"
        )

    cmds.file(input_path, **import_kwargs)
    skin_nodes_after = set(cmds.ls(type="skinCluster") or [])

    new_nodes = skin_nodes_after - skin_nodes_before
    deleted_nodes = skin_nodes_before - skin_nodes_after
    if new_nodes or deleted_nodes:
        raise RuntimeError(
            f"Skin cluster reuse gate failed for {normalized_case_name}. "
            f"skinCluster nodes were not reused in-place on second update import: "
            f"new={sorted(new_nodes)} deleted={sorted(deleted_nodes)}"
        )

def validate_animation_layer_import_gate(cmds, plugin_paths_by_format, sample_dir, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = ANIMATION_LAYER_IMPORT_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    input_path = resolve_input_path(sample_dir, case_name)
    format_name = detect_format(input_path)
    format_config = FORMAT_CONFIGS[format_name]
    plugin_path = plugin_paths_by_format.get(format_name)
    if not plugin_path:
        raise RuntimeError(f"Missing plugin for animation-layer gate format '{format_name}' while running case '{case_name}'")

    base_input_path = resolve_input_path(sample_dir, expectation["base_case"])

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, [plugin_path])

    base_import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["base_import_options"],
    )
    layer_import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["update_import_options"],
    )

    cmds.file(base_input_path, **base_import_kwargs)
    if not cmds.objExists(expectation["base_plug"]):
        raise RuntimeError(
            f"Animation layer gate failed for {normalized_case_name}. "
            f"missing base plug before layer import: {expectation['base_plug']}"
        )

    scene_counts_before = snapshot_scene_animation_only_counts(cmds)
    base_value = cmds.getAttr(expectation["base_plug"])
    cmds.file(input_path, **layer_import_kwargs)

    if not cmds.objExists(expectation["layer_name"]):
        raise RuntimeError(
            f"Animation layer gate failed for {normalized_case_name}. "
            f"missing expected animation layer: {expectation['layer_name']}"
        )

    scene_counts_after = snapshot_scene_animation_only_counts(cmds)
    if scene_counts_after != scene_counts_before:
        raise RuntimeError(
            f"Animation layer gate failed for {normalized_case_name}. "
            f"animationOnly import changed scene object counts: before={scene_counts_before} after={scene_counts_after}"
        )

    layer_curves = cmds.animLayer(expectation["layer_name"], query=True, animCurves=True) or []
    if len(layer_curves) < expectation["min_curve_count"]:
        raise RuntimeError(
            f"Animation layer gate failed for {normalized_case_name}. "
            f"expected at least {expectation['min_curve_count']} layer curves, got {layer_curves}"
        )

    cmds.animLayer(expectation["layer_name"], edit=True, mute=True)
    try:
        updated_value = cmds.getAttr(expectation["base_plug"])
    finally:
        cmds.animLayer(expectation["layer_name"], edit=True, mute=False)
    if abs(updated_value - base_value) > 1.0e-6:
        raise RuntimeError(
            f"Animation layer gate failed for {normalized_case_name}. "
            f"base plug was overwritten: before={base_value} after={updated_value}"
        )


def validate_source_delta_import_gate(cmds, plugin_paths_by_format, sample_dir, case_name):
    normalized_case_name = case_name.replace("\\", "/")
    expectation = SOURCE_DELTA_IMPORT_GATE_EXPECTATIONS.get(normalized_case_name)
    if not expectation:
        return

    input_path = resolve_input_path(sample_dir, case_name)
    format_name = detect_format(input_path)
    format_config = FORMAT_CONFIGS[format_name]
    plugin_path = plugin_paths_by_format.get(format_name)
    if not plugin_path:
        raise RuntimeError(f"Missing plugin for source-delta gate format '{format_name}' while running case '{case_name}'")

    base_input_path = resolve_input_path(sample_dir, expectation["base_case"])

    def _sample_plugs(sampled_plugs, times):
        values = {}
        for plug in sampled_plugs:
            if not cmds.objExists(plug):
                raise RuntimeError(
                    f"Source delta gate failed for {normalized_case_name}. "
                    f"missing expected plug: {plug}"
                )
            plug_values = []
            for time_value in times:
                cmds.currentTime(f"{time_value}sec", edit=True)
                value = cmds.getAttr(plug)
                if isinstance(value, (list, tuple)):
                    value = value[0]
                if isinstance(value, (list, tuple)):
                    value = value[0]
                plug_values.append(float(value))
            values[plug] = plug_values
        return values

    direct_import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["baseline_import_options"],
    )

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, [plugin_path])
    cmds.file(input_path, **direct_import_kwargs)
    direct_values = _sample_plugs(expectation["compare_plugs"], expectation["sample_times"])

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, [plugin_path])
    cmds.file(base_input_path, **direct_import_kwargs)
    cmds.file(
        input_path,
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
        options=expectation["delta_import_options"],
    )

    if not cmds.objExists(expectation["layer_name"]):
        raise RuntimeError(
            f"Source delta gate failed for {normalized_case_name}. "
            f"missing expected source-delta animation layer: {expectation['layer_name']}"
        )

    delta_values = _sample_plugs(expectation["compare_plugs"], expectation["sample_times"])
    for plug in expectation["compare_plugs"]:
        for baseline_value, delta_value, time_value in zip(direct_values[plug], delta_values[plug], expectation["sample_times"]):
            if abs(baseline_value - delta_value) > 1.0e-5:
                raise RuntimeError(
                    f"Source delta gate failed for {normalized_case_name}. "
                    f"value mismatch on {plug} at time {time_value}: baseline={baseline_value} delta={delta_value}"
                )


def verify_roundtrip(
    cmds,
    plugin_paths,
    import_type,
    exported_path,
    reference_meshes,
    reference_node_types,
    reference_skins,
    reference_blendshapes,
    reference_animations,
    mesh_marker_path,
    type_marker_path,
    skin_marker_path,
    blendshape_marker_path,
    animation_marker_path,
    import_options="",
):
    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, plugin_paths)

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    import_kwargs = dict(i=True, type=import_type, ignoreVersion=True, ra=True, mergeNamespacesOnClash=False, defaultNamespace=True)
    if import_options:
        import_kwargs["options"] = import_options
    cmds.file(exported_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    candidate_meshes = snapshot_scene_meshes()
    candidate_node_types = snapshot_imported_node_types(imported_roots)
    candidate_skins = snapshot_skin_bindings(cmds, imported_roots)
    candidate_blendshapes = snapshot_blendshape_bindings(cmds, imported_roots)
    candidate_animations = snapshot_animation_bindings(cmds, imported_roots)
    compare_mesh_snapshots(reference_meshes, candidate_meshes)
    compare_node_type_snapshots(reference_node_types, candidate_node_types)
    compare_skin_snapshots(reference_skins, candidate_skins)
    compare_blendshape_snapshots(reference_blendshapes, candidate_blendshapes)
    compare_animation_snapshots(reference_animations, candidate_animations)

    with open(mesh_marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")
    with open(type_marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")
    with open(skin_marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")
    with open(blendshape_marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")
    with open(animation_marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")


def collect_imported_roots(cmds, before_assemblies):
    after_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    imported_roots = sorted(after_assemblies - before_assemblies)
    return imported_roots


def detect_format(input_path):
    ext = os.path.splitext(input_path)[1].lower()
    for format_name, config in FORMAT_CONFIGS.items():
        if ext in config["extensions"]:
            return format_name
    raise RuntimeError(f"Unsupported sample extension: {input_path}")


def ensure_plugins_loaded(cmds, plugin_paths):
    for plugin_path in plugin_paths:
        if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
            cmds.loadPlugin(plugin_path)


def make_case_output_name(case_name):
    normalized_name = case_name.replace("\\", "/")
    root, ext = os.path.splitext(normalized_name)
    if ext.lower() in (".dmx", ".dmxb", ".dmxbin", ".smd"):
        normalized_name = root
    return normalized_name.replace("\\", "__").replace("/", "__")


def resolve_input_path(sample_dir, case_name):
    normalized_name = case_name.replace("\\", "/")
    root, ext = os.path.splitext(normalized_name)
    candidate_paths = []
    if ext.lower() in (".dmx", ".dmxb", ".dmxbin", ".smd"):
        candidate_paths.append(os.path.join(sample_dir, normalized_name))
    else:
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmx"))
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmxb"))
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmxbin"))
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.smd"))

    for candidate_path in candidate_paths:
        if os.path.isfile(candidate_path):
            return candidate_path

    raise RuntimeError(f"Missing sample file: {candidate_paths[0]}")


def run_case(cmds, plugin_paths_by_format, sample_dir, output_dir, case_name, import_options=""):
    label = case_name if not import_options else f"{case_name} [{import_options}]"
    sys.stdout.write(f"[maya_dmx_case] {label}\n")
    sys.stdout.flush()

    input_path = resolve_input_path(sample_dir, case_name)
    format_name = detect_format(input_path)
    format_config = FORMAT_CONFIGS[format_name]
    plugin_path = plugin_paths_by_format.get(format_name)
    if not plugin_path:
        raise RuntimeError(f"Missing plugin for format '{format_name}' while running case '{case_name}'")

    case_output_name = make_case_output_name(case_name)
    # Append a suffix for non-default import options so markers don't collide.
    options_suffix = ""
    if import_options:
        safe_options = import_options.replace("=", "").replace(";", "_").replace(" ", "")
        options_suffix = f".{safe_options}"
    import_animation_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.import_animgate.txt")
    append_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.append_gate.txt")
    update_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.update_gate.txt")
    topology_update_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.topology_update_gate.txt")
    skin_influence_update_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.skin_influence_update_gate.txt")
    skin_cluster_reuse_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.skin_cluster_reuse_gate.txt")
    animation_layer_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.animation_layer_gate.txt")
    source_delta_gate_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.source_delta_gate.txt")

    import_kwargs = dict(
        i=True,
        type=format_config["import_type"],
        ignoreVersion=True,
        ra=True,
        mergeNamespacesOnClash=False,
        defaultNamespace=True,
    )
    if import_options:
        import_kwargs["options"] = import_options

    cmds.file(new=True, force=True)
    ensure_plugins_loaded(cmds, [plugin_path])
    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, **import_kwargs)
    imported_roots = collect_imported_roots(cmds, before_assemblies)

    original_meshes = snapshot_scene_meshes()
    original_node_types = snapshot_imported_node_types(imported_roots)
    original_skins = snapshot_skin_bindings(cmds, imported_roots)
    original_blendshapes = snapshot_blendshape_bindings(cmds, imported_roots)
    original_animations = snapshot_animation_bindings(cmds, imported_roots)
    validate_animation_gate(case_name, original_animations)
    with open(import_animation_gate_marker, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")
    for export_variant in format_config["export_variants"]:
        cmds.file(new=True, force=True)
        ensure_plugins_loaded(cmds, [plugin_path])
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = collect_imported_roots(cmds, before_assemblies)
        exported_path = os.path.join(
            output_dir,
            f"{case_output_name}{options_suffix}.maya_export{export_variant['extension']}",
        )
        mesh_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.{export_variant['mesh_marker_suffix']}")
        type_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.{export_variant['type_marker_suffix']}")
        skin_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.{export_variant['skin_marker_suffix']}")
        blendshape_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.{export_variant['blendshape_marker_suffix']}")
        animation_marker = os.path.join(output_dir, f"{case_output_name}{options_suffix}.{export_variant['animation_marker_suffix']}")

        if imported_roots:
            cmds.select(imported_roots, replace=True)
        else:
            cmds.select(clear=True)
        cmds.file(rename=exported_path)
        export_kwargs = dict(force=True, exportSelected=True, type=format_config["export_type"])
        if export_variant["options"]:
            export_kwargs["options"] = export_variant["options"]
        cmds.file(**export_kwargs)

        verify_roundtrip(
            cmds,
            [plugin_path],
            format_config["import_type"],
            exported_path,
            original_meshes,
            original_node_types,
            original_skins,
            original_blendshapes,
            original_animations,
            mesh_marker,
            type_marker,
            skin_marker,
            blendshape_marker,
            animation_marker,
            import_options=import_options,
        )

    if not import_options:
        validate_append_gate(cmds, [plugin_path], format_config, input_path, case_name)
        with open(append_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_update_gate(cmds, [plugin_path], format_config, input_path, case_name)
        with open(update_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_topology_update_gate(cmds, [plugin_path], format_config, input_path, case_name)
        with open(topology_update_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_paired_update_gate(cmds, plugin_paths_by_format, sample_dir, case_name)
        validate_skin_influence_update_gate(cmds, [plugin_path], format_config, input_path, case_name)
        with open(skin_influence_update_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_skin_cluster_reuse_gate(cmds, [plugin_path], format_config, input_path, case_name)
        with open(skin_cluster_reuse_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_animation_layer_import_gate(cmds, plugin_paths_by_format, sample_dir, case_name)
        with open(animation_layer_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

        validate_source_delta_import_gate(cmds, plugin_paths_by_format, sample_dir, case_name)
        with open(source_delta_gate_marker, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

    # For samples that contain skinned meshes, automatically run a second pass with
    # applyAxisCorrection=0 to verify that roundtrip is consistent regardless of the
    # axis correction setting.  The no-correction import exports its own DMX and
    # re-imports with the same no-correction option so the reference and candidate
    # are always compared under matching settings.
    if format_name == "dmx" and original_skins and not import_options:
        run_case(
            cmds,
            plugin_paths_by_format,
            sample_dir,
            output_dir,
            case_name,
            import_options="applyAxisCorrection=0",
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", dest="plugin_dmx", required=True)
    parser.add_argument("--plugin-smd")
    parser.add_argument("--samples", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cases", nargs="+", required=True)
    args = parser.parse_args()

    args.plugin_dmx = os.path.abspath(args.plugin_dmx)
    if args.plugin_smd:
        args.plugin_smd = os.path.abspath(args.plugin_smd)
    args.samples = os.path.abspath(args.samples)
    args.output = os.path.abspath(args.output)
    os.makedirs(args.output, exist_ok=True)

    plugin_paths_by_format = {
        "dmx": args.plugin_dmx,
    }
    if args.plugin_smd:
        plugin_paths_by_format["smd"] = args.plugin_smd

    maya.standalone.initialize(name="python")
    try:
        import maya.cmds as cmds

        for case_name in args.cases:
            run_case(cmds, plugin_paths_by_format, args.samples, args.output, case_name)
    finally:
        maya.standalone.uninitialize()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"{exc}\n")
        sys.exit(1)
