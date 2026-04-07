import argparse
import os
import sys

import maya.standalone


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
            wrapper_names.add(parts[0])
            stripped_path = "|".join(parts[1:]) if len(parts) > 1 else ""
            if stripped_path:
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


def verify_roundtrip(
    cmds,
    plugin_path,
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
):
    cmds.file(new=True, force=True)
    if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
        cmds.loadPlugin(plugin_path)

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(exported_path, i=True, type="Valve DMX Import", ignoreVersion=True, ra=True, mergeNamespacesOnClash=False)
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


def make_case_output_name(case_name):
    normalized_name = case_name.replace("\\", "/")
    root, ext = os.path.splitext(normalized_name)
    if ext.lower() in (".dmx", ".dmxb", ".dmxbin"):
        normalized_name = root
    return normalized_name.replace("\\", "__").replace("/", "__")


def resolve_input_path(sample_dir, case_name):
    normalized_name = case_name.replace("\\", "/")
    root, ext = os.path.splitext(normalized_name)
    candidate_paths = []
    if ext.lower() in (".dmx", ".dmxb", ".dmxbin"):
        candidate_paths.append(os.path.join(sample_dir, normalized_name))
    else:
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmx"))
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmxb"))
        candidate_paths.append(os.path.join(sample_dir, f"{normalized_name}.dmxbin"))

    for candidate_path in candidate_paths:
        if os.path.isfile(candidate_path):
            return candidate_path

    raise RuntimeError(f"Missing sample file: {candidate_paths[0]}")


def run_case(cmds, plugin_path, sample_dir, output_dir, case_name):
    sys.stdout.write(f"[maya_dmx_case] {case_name}\n")
    sys.stdout.flush()

    input_path = resolve_input_path(sample_dir, case_name)
    case_output_name = make_case_output_name(case_name)
    exported_text = os.path.join(output_dir, f"{case_output_name}.maya_export.dmx")
    exported_binary = os.path.join(output_dir, f"{case_output_name}.maya_export.dmxb")
    roundtrip_text_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_meshcheck.txt")
    roundtrip_binary_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_meshcheck.txt")
    roundtrip_text_type_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_typecheck.txt")
    roundtrip_binary_type_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_typecheck.txt")
    roundtrip_text_skin_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_skincheck.txt")
    roundtrip_binary_skin_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_skincheck.txt")
    roundtrip_text_blendshape_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_blendshapecheck.txt")
    roundtrip_binary_blendshape_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_blendshapecheck.txt")
    roundtrip_text_animation_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_animcheck.txt")
    roundtrip_binary_animation_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_animcheck.txt")

    cmds.file(new=True, force=True)
    if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
        cmds.loadPlugin(plugin_path)

    before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    cmds.file(input_path, i=True, type="Valve DMX Import", ignoreVersion=True, ra=True, mergeNamespacesOnClash=False)
    imported_roots = collect_imported_roots(cmds, before_assemblies)
    if imported_roots:
        cmds.select(imported_roots, replace=True)
    else:
        cmds.select(clear=True)

    original_meshes = snapshot_scene_meshes()
    original_node_types = snapshot_imported_node_types(imported_roots)
    original_skins = snapshot_skin_bindings(cmds, imported_roots)
    original_blendshapes = snapshot_blendshape_bindings(cmds, imported_roots)
    original_animations = snapshot_animation_bindings(cmds, imported_roots)
    cmds.file(rename=exported_text)
    cmds.file(force=True, exportSelected=True, type="Valve DMX Export")

    cmds.file(rename=exported_binary)
    cmds.file(force=True, options="binary=1", exportSelected=True, type="Valve DMX Export")

    verify_roundtrip(
        cmds,
        plugin_path,
        exported_text,
        original_meshes,
        original_node_types,
        original_skins,
        original_blendshapes,
        original_animations,
        roundtrip_text_marker,
        roundtrip_text_type_marker,
        roundtrip_text_skin_marker,
        roundtrip_text_blendshape_marker,
        roundtrip_text_animation_marker,
    )
    verify_roundtrip(
        cmds,
        plugin_path,
        exported_binary,
        original_meshes,
        original_node_types,
        original_skins,
        original_blendshapes,
        original_animations,
        roundtrip_binary_marker,
        roundtrip_binary_type_marker,
        roundtrip_binary_skin_marker,
        roundtrip_binary_blendshape_marker,
        roundtrip_binary_animation_marker,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--samples", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cases", nargs="+", required=True)
    args = parser.parse_args()

    args.plugin = os.path.abspath(args.plugin)
    args.samples = os.path.abspath(args.samples)
    args.output = os.path.abspath(args.output)
    os.makedirs(args.output, exist_ok=True)

    maya.standalone.initialize(name="python")
    try:
        import maya.cmds as cmds

        for case_name in args.cases:
            run_case(cmds, args.plugin, args.samples, args.output, case_name)
    finally:
        maya.standalone.uninitialize()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"{exc}\n")
        sys.exit(1)
