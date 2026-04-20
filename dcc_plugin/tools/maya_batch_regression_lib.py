import json
import os
import sys
from dataclasses import dataclass


@dataclass
class ExportVariant:
    name: str
    extension: str
    options: str
    mesh_marker_suffix: str
    type_marker_suffix: str
    skin_marker_suffix: str
    blendshape_marker_suffix: str
    animation_marker_suffix: str


@dataclass
class FormatConfig:
    name: str
    extensions: tuple
    import_type: str
    export_type: str
    export_variants: list


class RegressionConfig:
    def __init__(self, config_path):
        with open(config_path, "r", encoding="utf-8") as config_file:
            raw = json.load(config_file)

        self.default_cases = raw["default_cases"]
        self._sections = {}
        self.format_configs = {}

        for format_name, format_data in raw["format_configs"].items():
            export_variants = [ExportVariant(**variant_data) for variant_data in format_data["export_variants"]]
            self.format_configs[format_name] = FormatConfig(
                name=format_name,
                extensions=tuple(format_data["extensions"]),
                import_type=format_data["import_type"],
                export_type=format_data["export_type"],
                export_variants=export_variants,
            )

        for key, value in raw.items():
            if key in ("default_cases", "format_configs"):
                continue
            self._sections[key] = value

    @staticmethod
    def normalize_case_name(case_name):
        return case_name.replace("\\", "/")

    def get_format_config(self, format_name):
        return self.format_configs[format_name]

    def get_case_expectation(self, section_name, case_name):
        return self._sections.get(section_name, {}).get(self.normalize_case_name(case_name))


class MayaRegressionContext:
    def __init__(self, cmds, config, plugin_paths_by_format, sample_dir, output_dir):
        self.cmds = cmds
        self.config = config
        self.plugin_paths_by_format = plugin_paths_by_format
        self.sample_dir = os.path.abspath(sample_dir)
        self.output_dir = os.path.abspath(output_dir)

    def ensure_plugins_loaded(self, plugin_paths):
        for plugin_path in plugin_paths:
            if not self.cmds.pluginInfo(plugin_path, query=True, loaded=True):
                self.cmds.loadPlugin(plugin_path)

    def ensure_plugin_loaded_for_format(self, format_name):
        plugin_path = self.plugin_paths_by_format.get(format_name)
        if not plugin_path:
            raise RuntimeError(f"Missing plugin for format '{format_name}'")
        self.ensure_plugins_loaded([plugin_path])
        return plugin_path

    def detect_format(self, input_path):
        ext = os.path.splitext(input_path)[1].lower()
        for format_name, format_config in self.config.format_configs.items():
            if ext in format_config.extensions:
                return format_name
        raise RuntimeError(f"Unsupported sample extension: {input_path}")

    def resolve_input_path(self, case_name):
        normalized_name = self.config.normalize_case_name(case_name)
        root, ext = os.path.splitext(normalized_name)
        candidate_paths = []
        if ext.lower() in (".dmx", ".dmxb", ".dmxbin", ".smd", ".vta"):
            candidate_paths.append(os.path.join(self.sample_dir, normalized_name))
        else:
            candidate_paths.append(os.path.join(self.sample_dir, f"{normalized_name}.dmx"))
            candidate_paths.append(os.path.join(self.sample_dir, f"{normalized_name}.dmxb"))
            candidate_paths.append(os.path.join(self.sample_dir, f"{normalized_name}.dmxbin"))
            candidate_paths.append(os.path.join(self.sample_dir, f"{normalized_name}.smd"))
            candidate_paths.append(os.path.join(self.sample_dir, f"{normalized_name}.vta"))

        for candidate_path in candidate_paths:
            if os.path.isfile(candidate_path):
                return candidate_path

        raise RuntimeError(f"Missing sample file: {candidate_paths[0]}")

    def make_case_output_name(self, case_name):
        normalized_name = self.config.normalize_case_name(case_name)
        root, ext = os.path.splitext(normalized_name)
        if ext.lower() in (".dmx", ".dmxb", ".dmxbin", ".smd", ".vta"):
            normalized_name = root
        return normalized_name.replace("\\", "__").replace("/", "__")

    def build_import_kwargs(self, format_config, options=""):
        kwargs = dict(
            i=True,
            type=format_config.import_type,
            ignoreVersion=True,
            ra=True,
            mergeNamespacesOnClash=False,
            defaultNamespace=True,
        )
        if options:
            kwargs["options"] = options
        return kwargs

    def build_export_kwargs(self, format_config, export_variant):
        kwargs = dict(force=True, exportSelected=True, type=format_config.export_type)
        if export_variant.options:
            kwargs["options"] = export_variant.options
        return kwargs

    def collect_imported_roots(self, before_assemblies):
        after_assemblies = set(self.cmds.ls(assemblies=True, long=True) or [])
        return sorted(after_assemblies - before_assemblies)

    def write_marker(self, marker_path):
        with open(marker_path, "w", encoding="utf-8") as marker_file:
            marker_file.write("ok\n")

    def resolve_expected_plug(self, plug_expectation):
        explicit_plug = plug_expectation.get("plug")
        if explicit_plug:
            return explicit_plug if self.cmds.objExists(explicit_plug) else None

        plug_suffix = plug_expectation.get("plug_suffix")
        if not plug_suffix:
            return None

        if "." not in plug_suffix:
            raise RuntimeError(f"Invalid plug_suffix without attribute name: {plug_suffix}")

        node_suffix, attribute_name = plug_suffix.rsplit(".", 1)
        candidate_nodes = self.cmds.ls("*" + node_suffix, long=True) or []
        candidate_plugs = []
        for node_name in candidate_nodes:
            candidate_plug = node_name + "." + attribute_name
            if self.cmds.objExists(candidate_plug):
                candidate_plugs.append(candidate_plug)

        unique_candidates = sorted(set(candidate_plugs))
        if len(unique_candidates) != 1:
            return None
        return unique_candidates[0]

    def find_single_node_type_matches(self, expectation):
        candidate_nodes = self.cmds.ls(type=expectation["type"]) or []
        exact_name = expectation.get("name")
        if exact_name:
            return [node_name for node_name in candidate_nodes if node_name == exact_name]
        name_suffix = expectation.get("name_suffix")
        if name_suffix:
            return [node_name for node_name in candidate_nodes if node_name.endswith(name_suffix)]
        return candidate_nodes

    def capture_warning_messages(self, callback):
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

    def sample_plugs(self, plugs, times):
        sampled_values = {}
        for plug_name in plugs:
            if not self.cmds.objExists(plug_name):
                raise RuntimeError(f"Missing expected plug: {plug_name}")
            values = []
            for time_value in times:
                self.cmds.currentTime(f"{time_value}sec", edit=True)
                value = self.cmds.getAttr(plug_name)
                if isinstance(value, (list, tuple)):
                    value = value[0]
                if isinstance(value, (list, tuple)):
                    value = value[0]
                values.append(float(value))
            sampled_values[plug_name] = values
        return sampled_values

    def sample_layer_curve_outputs(self, layer_name, plug_name, times):
        curve_names = self.cmds.animLayer(layer_name, query=True, findCurveForPlug=plug_name) or []
        if isinstance(curve_names, str):
            curve_names = [curve_names]
        if len(curve_names) != 1:
            raise RuntimeError(
                f"Expected a single curve for plug {plug_name} on layer {layer_name}, got {curve_names}"
            )

        curve_name = curve_names[0]
        if not self.cmds.objExists(curve_name):
            raise RuntimeError(f"Missing animation-layer curve: {curve_name}")

        values = []
        for time_value in times:
            self.cmds.currentTime(f"{time_value}sec", edit=True)
            value = self.cmds.getAttr(curve_name + ".output")
            if isinstance(value, (list, tuple)):
                value = value[0]
            if isinstance(value, (list, tuple)):
                value = value[0]
            values.append(float(value))
        return values

    def set_animation_layer_mute_states(self, layer_states):
        for layer_name, muted in layer_states.items():
            self.cmds.animLayer(layer_name, edit=True, mute=muted)


class SnapshotUtils:
    @staticmethod
    def snapshot_scene_animation_only_counts(cmds):
        return {
            "joint_count": len(cmds.ls(type="joint", long=True) or []),
            "mesh_count": len(cmds.ls(type="mesh", long=True) or []),
            "blendshape_count": len(cmds.ls(type="blendShape", long=True) or []),
            "transform_count": len(cmds.ls(type="transform", long=True) or []),
        }

    @staticmethod
    def snapshot_imported_node_types(cmds, root_paths):
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

    @staticmethod
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

    @staticmethod
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

    @staticmethod
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
            for node_name in [root_path] + descendant_transforms:
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

    @staticmethod
    def snapshot_selected_animation_bindings(cmds, plug_names):
        animation_snapshots = {}
        for plug_name in plug_names:
            if "." not in plug_name or not cmds.objExists(plug_name):
                continue
            node_name, attribute_name = plug_name.rsplit(".", 1)
            source_connections = cmds.listConnections(plug_name, source=True, destination=False, plugs=True) or []
            key_count = cmds.keyframe(node_name, attribute=attribute_name, query=True, keyframeCount=True) or 0
            if not source_connections and not key_count:
                continue
            animation_snapshots[plug_name] = {
                "key_count": int(key_count),
                "sources": sorted(source_connections),
            }
        return animation_snapshots

    @staticmethod
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

    @staticmethod
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


class SnapshotComparator:
    @staticmethod
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

    @staticmethod
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

    @classmethod
    def _compare_wrapper_aware(cls, reference_nodes, candidate_nodes, compare_exact):
        candidate_variants = [candidate_nodes]
        stripped_candidate = cls.strip_single_wrapper_snapshot(candidate_nodes)
        if stripped_candidate:
            candidate_variants.append(stripped_candidate)

        reference_variants = [reference_nodes]
        stripped_reference = cls.strip_single_wrapper_snapshot(reference_nodes)
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

    @classmethod
    def compare_node_type_snapshots(cls, reference_nodes, candidate_nodes):
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

        cls._compare_wrapper_aware(reference_nodes, candidate_nodes, compare_exact)

    @classmethod
    def compare_skin_snapshots(cls, reference_skins, candidate_skins):
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

        cls._compare_wrapper_aware(reference_skins, candidate_skins, compare_exact)

    @classmethod
    def compare_blendshape_snapshots(cls, reference_blendshapes, candidate_blendshapes):
        def compare_exact(lhs_blendshapes, rhs_blendshapes):
            if set(lhs_blendshapes.keys()) != set(rhs_blendshapes.keys()):
                missing = sorted(set(lhs_blendshapes.keys()) - set(rhs_blendshapes.keys()))
                extra = sorted(set(rhs_blendshapes.keys()) - set(lhs_blendshapes.keys()))
                raise RuntimeError(f"BlendShape set mismatch. Missing={missing} Extra={extra}")

            for mesh_key in sorted(lhs_blendshapes.keys()):
                if lhs_blendshapes[mesh_key]["target_count"] != rhs_blendshapes[mesh_key]["target_count"]:
                    raise RuntimeError(
                        f"BlendShape target count mismatch for {mesh_key}. "
                        f"expected={lhs_blendshapes[mesh_key]['target_count']} actual={rhs_blendshapes[mesh_key]['target_count']}"
                    )

        cls._compare_wrapper_aware(reference_blendshapes, candidate_blendshapes, compare_exact)

    @classmethod
    def compare_animation_snapshots(cls, reference_animations, candidate_animations):
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


class GateValidator:
    def __init__(self, context):
        self.ctx = context

    def validate_animation_gate(self, case_name, animation_snapshots):
        expectation = self.ctx.config.get_case_expectation("animation_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
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

    def validate_append_gate(self, case_name, format_config, input_path):
        expectation = self.ctx.config.get_case_expectation("append_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["import_options"])
        cmds.file(input_path, **import_kwargs)

        for retain_value in expectation.get("retain_values", []):
            plug_name = self.ctx.resolve_expected_plug(retain_value)
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
            matching_nodes = self.ctx.find_single_node_type_matches(single_node_type)
            if len(matching_nodes) != 1:
                raise RuntimeError(
                    f"Append gate failed for {normalized_case_name}. "
                    f"expected exactly one {single_node_type['type']} matching "
                    f"{single_node_type.get('name') or single_node_type.get('name_suffix')}, got {matching_nodes}"
                )

        for retain_value in expectation.get("retain_values", []):
            plug_name = retain_value.get("_resolved_plug") or self.ctx.resolve_expected_plug(retain_value)
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

    def validate_update_gate(self, case_name, format_config, input_path):
        expectation = self.ctx.config.get_case_expectation("update_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["import_options"])

        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"Update gate failed for {normalized_case_name}. first import produced no DAG roots"
            )

        reference_meshes = SnapshotUtils.snapshot_scene_meshes()
        reference_node_types = SnapshotUtils.snapshot_imported_node_types(cmds, imported_roots)
        reference_skins = SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots)
        reference_blendshapes = SnapshotUtils.snapshot_blendshape_bindings(cmds, imported_roots)
        original_values = {}
        for overwrite_value in expectation.get("overwrite_values", []):
            plug_name = self.ctx.resolve_expected_plug(overwrite_value)
            if not plug_name:
                raise RuntimeError(
                    f"Update gate failed for {normalized_case_name}. "
                    f"missing plug before second update: {overwrite_value.get('plug') or overwrite_value.get('plug_suffix')}"
                )
            overwrite_value["_resolved_plug"] = plug_name
            original_values[plug_name] = cmds.getAttr(plug_name)
            cmds.setAttr(plug_name, overwrite_value["value"])

        cmds.file(input_path, **import_kwargs)

        current_imported_roots = self.ctx.collect_imported_roots(before_assemblies)
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
            matching_nodes = self.ctx.find_single_node_type_matches(single_node_type)
            if len(matching_nodes) != 1:
                raise RuntimeError(
                    f"Update gate failed for {normalized_case_name}. "
                    f"expected exactly one {single_node_type['type']} matching "
                    f"{single_node_type.get('name') or single_node_type.get('name_suffix')}, got {matching_nodes}"
                )

        for overwrite_value in expectation.get("overwrite_values", []):
            plug_name = overwrite_value.get("_resolved_plug") or self.ctx.resolve_expected_plug(overwrite_value)
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

        SnapshotComparator.compare_mesh_snapshots(reference_meshes, SnapshotUtils.snapshot_scene_meshes())
        SnapshotComparator.compare_node_type_snapshots(
            reference_node_types,
            SnapshotUtils.snapshot_imported_node_types(cmds, imported_roots),
        )
        SnapshotComparator.compare_skin_snapshots(
            reference_skins,
            SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots),
        )
        SnapshotComparator.compare_blendshape_snapshots(
            reference_blendshapes,
            SnapshotUtils.snapshot_blendshape_bindings(cmds, imported_roots),
        )

    def validate_topology_update_gate(self, case_name, format_config, input_path):
        expectation = self.ctx.config.get_case_expectation("topology_update_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["import_options"])

        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        mesh_path = SnapshotUtils.find_single_visible_mesh_path(cmds, imported_roots)
        reference_meshes = SnapshotUtils.snapshot_scene_meshes()

        cmds.xform(f"{mesh_path}.vtx[0]", relative=True, objectSpace=True, translation=(1.0, 0.0, 0.0))
        warning_messages = self.ctx.capture_warning_messages(lambda: cmds.file(input_path, **import_kwargs))
        if any(expectation["warning_substrings"][0] in warning for warning in warning_messages):
            raise RuntimeError(
                f"Topology update gate failed for {normalized_case_name}. "
                f"same-topology update unexpectedly produced topology mismatch warning: {warning_messages}"
            )
        SnapshotComparator.compare_mesh_snapshots(reference_meshes, SnapshotUtils.snapshot_scene_meshes())

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        mesh_path = SnapshotUtils.find_single_visible_mesh_path(cmds, imported_roots)

        cmds.polySubdivideFacet(f"{mesh_path}.f[0]", divisions=1, constructionHistory=False)
        mutated_meshes = SnapshotUtils.snapshot_scene_meshes()
        warning_messages = self.ctx.capture_warning_messages(lambda: cmds.file(input_path, **import_kwargs))
        if not any(
            expectation["warning_substrings"][0] in warning and expectation["warning_substrings"][1] in warning
            for warning in warning_messages
        ):
            raise RuntimeError(
                f"Topology update gate failed for {normalized_case_name}. "
                f"missing topology mismatch warning, got {warning_messages}"
            )
        SnapshotComparator.compare_mesh_snapshots(mutated_meshes, SnapshotUtils.snapshot_scene_meshes())

    def validate_paired_update_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("paired_update_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])

        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        base_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["base_import_options"])
        update_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["update_import_options"])

        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(base_input_path, **base_import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"Paired update gate failed for {normalized_case_name}. base import produced no DAG roots"
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
                    f"Paired update gate failed for {normalized_case_name}. plug {plug_name} did not receive animation"
                )

    def validate_skin_influence_update_gate(self, case_name, format_config, input_path):
        expectation = self.ctx.config.get_case_expectation("skin_influence_update_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["import_options"])

        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        reference_skins = SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots)
        if not reference_skins:
            raise RuntimeError(
                f"Skin influence update gate failed for {normalized_case_name}. initial import produced no skinCluster"
            )

        target_skin = next(iter(reference_skins.values()))
        if target_skin["influence_count"] <= 1:
            raise RuntimeError(
                f"Skin influence update gate failed for {normalized_case_name}. "
                f"need at least two influences, got {target_skin['influence_count']}"
            )

        removed_influence = target_skin["influences"][-1]
        cmds.skinCluster(target_skin["skin_cluster"], edit=True, removeInfluence=removed_influence)
        mutated_skins = SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots)
        mutated_target_skin = next(iter(mutated_skins.values()))
        if removed_influence in mutated_target_skin.get("influences", []):
            raise RuntimeError(
                f"Skin influence update gate failed for {normalized_case_name}. "
                f"failed to remove influence before update: {removed_influence}"
            )

        cmds.file(input_path, **import_kwargs)
        SnapshotComparator.compare_skin_snapshots(reference_skins, SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots))

    def validate_skin_cluster_reuse_gate(self, case_name, format_config, input_path):
        expectation = self.ctx.config.get_case_expectation("skin_cluster_reuse_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[self.ctx.detect_format(input_path)]])
        import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["import_options"])

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

    def validate_animation_layer_import_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("animation_layer_import_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])

        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        base_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["base_import_options"])
        layer_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["update_import_options"])

        cmds.file(base_input_path, **base_import_kwargs)
        if not cmds.objExists(expectation["base_plug"]):
            raise RuntimeError(
                f"Animation layer gate failed for {normalized_case_name}. "
                f"missing base plug before layer import: {expectation['base_plug']}"
            )

        scene_counts_before = SnapshotUtils.snapshot_scene_animation_only_counts(cmds)
        base_value = cmds.getAttr(expectation["base_plug"])
        cmds.file(input_path, **layer_import_kwargs)

        if not cmds.objExists(expectation["layer_name"]):
            raise RuntimeError(
                f"Animation layer gate failed for {normalized_case_name}. "
                f"missing expected animation layer: {expectation['layer_name']}"
            )

        scene_counts_after = SnapshotUtils.snapshot_scene_animation_only_counts(cmds)
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

    def validate_animation_layer_replace_reimport_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("animation_layer_replace_reimport_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])

        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        base_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["base_import_options"])
        layer_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["layer_import_options"])

        cmds.file(base_input_path, **base_import_kwargs)
        cmds.file(input_path, **layer_import_kwargs)
        first_layer_curves = cmds.animLayer(expectation["layer_name"], query=True, animCurves=True) or []
        first_samples = {
            plug_name: self.ctx.sample_layer_curve_outputs(expectation["layer_name"], plug_name, expectation["sample_times"])
            for plug_name in expectation["compare_plugs"]
        }

        cmds.file(input_path, **layer_import_kwargs)
        second_layer_curves = cmds.animLayer(expectation["layer_name"], query=True, animCurves=True) or []
        if len(first_layer_curves) != len(second_layer_curves):
            raise RuntimeError(
                f"Animation layer replace gate failed for {normalized_case_name}. "
                f"curve count changed after second replace import: before={len(first_layer_curves)} after={len(second_layer_curves)}"
            )

        for plug_name in expectation["compare_plugs"]:
            second_samples = self.ctx.sample_layer_curve_outputs(expectation["layer_name"], plug_name, expectation["sample_times"])
            for first_value, second_value, time_value in zip(first_samples[plug_name], second_samples, expectation["sample_times"]):
                if abs(first_value - second_value) > 1.0e-5:
                    raise RuntimeError(
                        f"Animation layer replace gate failed for {normalized_case_name}. "
                        f"value drift on {plug_name} at time {time_value}: first={first_value} second={second_value}"
                    )

    def validate_animation_layer_new_layers_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("animation_layer_new_layers_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])
        format_name = self.ctx.detect_format(base_input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        cmds = self.ctx.cmds

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        base_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["base_import_options"])
        cmds.file(base_input_path, **base_import_kwargs)

        scene_counts_before = SnapshotUtils.snapshot_scene_animation_only_counts(cmds)
        base_values = self.ctx.sample_plugs([expectation["base_plug"]], expectation["sample_times"])[expectation["base_plug"]]

        imported_layers = []
        for layer_import in expectation["layer_imports"]:
            layer_input_path = self.ctx.resolve_input_path(layer_import["case"])
            layer_import_kwargs = self.ctx.build_import_kwargs(format_config, layer_import["options"])
            cmds.file(layer_input_path, **layer_import_kwargs)
            layer_name = layer_import["layer_name"]
            imported_layers.append(layer_name)
            if not cmds.objExists(layer_name):
                raise RuntimeError(
                    f"Animation layer multi-new gate failed for {normalized_case_name}. missing layer {layer_name}"
                )
            layer_curves = cmds.animLayer(layer_name, query=True, animCurves=True) or []
            if len(layer_curves) < layer_import.get("min_curve_count", 1):
                raise RuntimeError(
                    f"Animation layer multi-new gate failed for {normalized_case_name}. "
                    f"layer {layer_name} has insufficient curves: {layer_curves}"
                )

        if len(set(imported_layers)) != len(imported_layers):
            raise RuntimeError(
                f"Animation layer multi-new gate failed for {normalized_case_name}. layer names were not unique: {imported_layers}"
            )

        scene_counts_after = SnapshotUtils.snapshot_scene_animation_only_counts(cmds)
        if scene_counts_before != scene_counts_after:
            raise RuntimeError(
                f"Animation layer multi-new gate failed for {normalized_case_name}. "
                f"animationOnly import changed scene counts: before={scene_counts_before} after={scene_counts_after}"
            )

        original_mute_states = {
            layer_name: bool(cmds.animLayer(layer_name, query=True, mute=True))
            for layer_name in imported_layers
        }
        try:
            self.ctx.set_animation_layer_mute_states({layer_name: True for layer_name in imported_layers})
            muted_values = self.ctx.sample_plugs([expectation["base_plug"]], expectation["sample_times"])[expectation["base_plug"]]
            for muted_value, base_value, time_value in zip(muted_values, base_values, expectation["sample_times"]):
                if abs(muted_value - base_value) > 1.0e-6:
                    raise RuntimeError(
                        f"Animation layer multi-new gate failed for {normalized_case_name}. "
                        f"muted layers changed base value at time {time_value}: base={base_value} muted={muted_value}"
                    )

            layer_only_values = []
            for layer_name in imported_layers:
                states = {other_layer: other_layer != layer_name for other_layer in imported_layers}
                self.ctx.set_animation_layer_mute_states(states)
                layer_values = self.ctx.sample_plugs([expectation["base_plug"]], expectation["sample_times"])[expectation["base_plug"]]
                layer_only_values.append(layer_values)

            if not any(
                abs(layer_value - base_value) > 1.0e-6
                for values in layer_only_values
                for layer_value, base_value in zip(values, base_values)
            ):
                raise RuntimeError(
                    f"Animation layer multi-new gate failed for {normalized_case_name}. "
                    f"all imported layers evaluated identically to base animation"
                )
        finally:
            self.ctx.set_animation_layer_mute_states(original_mute_states)

    def validate_source_delta_import_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("source_delta_import_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])
        cmds = self.ctx.cmds

        direct_import_kwargs = self.ctx.build_import_kwargs(format_config, expectation["baseline_import_options"])

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        cmds.file(input_path, **direct_import_kwargs)
        input_values = self.ctx.sample_plugs(expectation["compare_plugs"], expectation["sample_times"])

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        cmds.file(base_input_path, **direct_import_kwargs)
        baseline_values = self.ctx.sample_plugs(expectation["compare_plugs"], expectation["sample_times"])

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        cmds.file(base_input_path, **direct_import_kwargs)
        cmds.file(input_path, **self.ctx.build_import_kwargs(format_config, expectation["delta_import_options"]))

        if not cmds.objExists(expectation["layer_name"]):
            raise RuntimeError(
                f"Source delta gate failed for {normalized_case_name}. "
                f"missing expected source-delta animation layer: {expectation['layer_name']}"
            )

        is_override = cmds.animLayer(expectation["layer_name"], query=True, override=True)
        if is_override:
            raise RuntimeError(
                f"Source delta gate failed for {normalized_case_name}. "
                f"source-delta layer is still override: {expectation['layer_name']}"
            )

        layer_curves = cmds.animLayer(expectation["layer_name"], query=True, animCurves=True) or []
        if len(layer_curves) < len(expectation["compare_plugs"]):
            raise RuntimeError(
                f"Source delta gate failed for {normalized_case_name}. "
                f"insufficient animCurves were attached to source-delta layer: "
                f"{expectation['layer_name']} curves={layer_curves}"
            )

        expect_pre_subtract = "sourcedeltamode=presubtract" in expectation["delta_import_options"].lower()
        for plug_name in expectation["compare_plugs"]:
            layer_values = self.ctx.sample_layer_curve_outputs(
                expectation["layer_name"],
                plug_name,
                expectation["sample_times"],
            )
            for direct_value, baseline_value, layer_value, time_value in zip(
                input_values[plug_name],
                baseline_values[plug_name],
                layer_values,
                expectation["sample_times"],
            ):
                expected_value = baseline_value - direct_value if expect_pre_subtract else direct_value - baseline_value
                if abs(expected_value - layer_value) > 1.0e-5:
                    raise RuntimeError(
                        f"Source delta gate failed for {normalized_case_name}. "
                        f"value mismatch on {plug_name} at time {time_value}: expected={expected_value} layer={layer_value}"
                    )

    @staticmethod
    def parse_float_triplet(text):
        values = [float(component) for component in text.strip().split()]
        if len(values) != 3:
            raise RuntimeError(f"Expected float triplet, got: {text}")
        return values

    @staticmethod
    def parse_float_quad(text):
        values = [float(component) for component in text.strip().split()]
        if len(values) != 4:
            raise RuntimeError(f"Expected float quad, got: {text}")
        return values

    @staticmethod
    def assert_close_triplet(actual, expected, label, tolerance=1.0e-3):
        if len(actual) != 3 or len(expected) != 3:
            raise RuntimeError(f"{label}: invalid triplet lengths actual={actual} expected={expected}")
        for actual_value, expected_value in zip(actual, expected):
            if abs(actual_value - expected_value) > tolerance:
                raise RuntimeError(f"{label}: value mismatch actual={actual} expected={expected}")

    @staticmethod
    def parse_export_option_map(options):
        option_map = {}
        for option_entry in (options or "").split(";"):
            stripped = option_entry.strip()
            if not stripped or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            option_map[key.strip().lower()] = value.strip()
        return option_map

    @classmethod
    def build_correction_matrix(cls, options):
        import math
        import maya.api.OpenMaya as om

        option_map = cls.parse_export_option_map(options)
        translate = om.MVector(
            float(option_map.get("translatex", "0")),
            float(option_map.get("translatey", "0")),
            float(option_map.get("translatez", "0")),
        )
        rotation = om.MEulerRotation(
            math.radians(float(option_map.get("rotatex", "0"))),
            math.radians(float(option_map.get("rotatey", "0"))),
            math.radians(float(option_map.get("rotatez", "0"))),
            om.MEulerRotation.kXYZ,
        )
        scale = (
            float(option_map.get("scalex", "1")),
            float(option_map.get("scaley", "1")),
            float(option_map.get("scalez", "1")),
        )

        transform = om.MTransformationMatrix()
        transform.setTranslation(translate, om.MSpace.kTransform)
        transform.setRotation(rotation.asQuaternion())
        transform.setScale(scale, om.MSpace.kTransform)
        return transform.asMatrix()

    @staticmethod
    def resolve_unique_node_by_suffix(cmds, suffix):
        candidate_matches = cmds.ls("*|" + suffix, long=True) or []
        if not candidate_matches:
            candidate_matches = cmds.ls(suffix, long=True) or []
        unique_matches = sorted(set(candidate_matches))
        if len(unique_matches) != 1:
            raise RuntimeError(f"Expected exactly one node ending with {suffix}, got {unique_matches}")
        return unique_matches[0]

    @staticmethod
    def resolve_unique_mesh_by_suffix(cmds, mesh_suffix):
        candidate_matches = [
            mesh_path
            for mesh_path in (cmds.ls(type="mesh", long=True) or [])
            if mesh_path.endswith(mesh_suffix) and not cmds.getAttr(mesh_path + ".intermediateObject")
        ]
        unique_matches = sorted(set(candidate_matches))
        if len(unique_matches) != 1:
            raise RuntimeError(f"Expected exactly one visible mesh ending with {mesh_suffix}, got {unique_matches}")
        return unique_matches[0]

    @staticmethod
    def sample_node_world_frame(cmds, node_path):
        import maya.api.OpenMaya as om

        selection = om.MSelectionList()
        selection.add(node_path)
        world_matrix = selection.getDagPath(0).inclusiveMatrix()
        sample_points = {
            "origin": om.MPoint(0.0, 0.0, 0.0) * world_matrix,
            "x_axis": om.MPoint(1.0, 0.0, 0.0) * world_matrix,
            "y_axis": om.MPoint(0.0, 1.0, 0.0) * world_matrix,
            "z_axis": om.MPoint(0.0, 0.0, 1.0) * world_matrix,
        }
        return {
            sample_name: [sample_point.x, sample_point.y, sample_point.z]
            for sample_name, sample_point in sample_points.items()
        }

    @staticmethod
    def sample_mesh_vertex_positions(cmds, mesh_path, vertex_indices):
        sampled_positions = {}
        for vertex_index in vertex_indices:
            sampled_positions[vertex_index] = list(
                cmds.pointPosition(f"{mesh_path}.vtx[{vertex_index}]", world=True)
            )
        return sampled_positions

    @staticmethod
    def sample_mesh_vertex_normals(cmds, mesh_path, vertex_indices):
        import maya.api.OpenMaya as om

        selection = om.MSelectionList()
        selection.add(mesh_path)
        mesh_dag_path = selection.getDagPath(0)
        mesh_fn = om.MFnMesh(mesh_dag_path)

        sampled_normals = {}
        for vertex_index in vertex_indices:
            normal = mesh_fn.getVertexNormal(vertex_index, True, om.MSpace.kWorld)
            sampled_normals[vertex_index] = [normal.x, normal.y, normal.z]
        return sampled_normals

    @classmethod
    def apply_matrix_to_triplet(cls, matrix, triplet):
        import maya.api.OpenMaya as om

        transformed_point = om.MPoint(triplet[0], triplet[1], triplet[2]) * matrix
        return [transformed_point.x, transformed_point.y, transformed_point.z]

    @classmethod
    def apply_matrix_to_direction_triplet(cls, matrix, triplet):
        import maya.api.OpenMaya as om

        transformed_direction = om.MVector(triplet[0], triplet[1], triplet[2]) * matrix.inverse().transpose()
        if transformed_direction.length() > 1.0e-8:
            transformed_direction.normalize()
        return [transformed_direction.x, transformed_direction.y, transformed_direction.z]

    def parse_exported_dmx_transform_gate_data(self, exported_path):
        with open(exported_path, "r", encoding="utf-8") as exported_file:
            contents = exported_file.read()

        lines = contents.splitlines()
        up_axis = None
        first_transform_position = None
        bind_positions = []
        transform_data_by_name = {}
        element_stack = []

        for line in lines:
            stripped = line.strip()
            if stripped in ('"DmeDag"', '"DmeJoint"'):
                element_stack.append({
                    "kind": "dag",
                    "name": None,
                })
                continue
            if stripped.endswith('"DmeTransform"'):
                parent_name = None
                for element in reversed(element_stack):
                    if element["kind"] == "dag" and element.get("name"):
                        parent_name = element["name"]
                        break
                element_stack.append({
                    "kind": "transform",
                    "parent_name": parent_name,
                    "position": None,
                    "orientation": None,
                })
                continue
            if stripped == "{":
                continue
            if stripped == "}":
                if element_stack:
                    closed_element = element_stack.pop()
                    if (
                        closed_element["kind"] == "transform"
                        and closed_element.get("parent_name")
                        and (closed_element.get("position") is not None or closed_element.get("orientation") is not None)
                    ):
                        transform_data_by_name[closed_element["parent_name"]] = {
                            "position": closed_element.get("position"),
                            "orientation": closed_element.get("orientation"),
                        }
                continue
            if up_axis is None and stripped.startswith('"upAxis" "string"'):
                up_axis = stripped.split('"')[-2]
            if first_transform_position is None and stripped.startswith('"position" "vector3"'):
                first_transform_position = self.parse_float_triplet(stripped.split('"')[-2])
            if element_stack:
                current_element = element_stack[-1]
                if current_element["kind"] == "dag" and current_element.get("name") is None and stripped.startswith('"name" "string"'):
                    current_element["name"] = stripped.split('"')[-2]
                elif current_element["kind"] == "transform":
                    if current_element.get("position") is None and stripped.startswith('"position" "vector3"'):
                        current_element["position"] = self.parse_float_triplet(stripped.split('"')[-2])
                    elif current_element.get("orientation") is None and stripped.startswith('"orientation" "quaternion"'):
                        current_element["orientation"] = self.parse_float_quad(stripped.split('"')[-2])
            if stripped == '"positions" "vector3_array"':
                break

        bind_state_start = contents.find('"bindState" "DmeVertexData"')
        if bind_state_start >= 0:
            positions_start = contents.find('"positions" "vector3_array"', bind_state_start)
            if positions_start >= 0:
                bracket_start = contents.find("[", positions_start)
                if bracket_start >= 0:
                    bracket_depth = 0
                    bracket_end = -1
                    for index in range(bracket_start, len(contents)):
                        character = contents[index]
                        if character == "[":
                            bracket_depth += 1
                        elif character == "]":
                            bracket_depth -= 1
                            if bracket_depth == 0:
                                bracket_end = index
                                break

                    if bracket_end >= 0:
                        positions_block = contents[bracket_start + 1:bracket_end]
                        for raw_line in positions_block.splitlines():
                            stripped = raw_line.strip().rstrip(",")
                            if not stripped:
                                continue
                            if stripped.startswith('"') and stripped.endswith('"'):
                                bind_positions.append(self.parse_float_triplet(stripped.strip('"')))

        return {
            "up_axis": up_axis,
            "transform_position": first_transform_position,
            "bind_positions": bind_positions,
            "transform_data_by_name": transform_data_by_name,
        }

    def parse_exported_smd_transform_gate_data(self, exported_path):
        with open(exported_path, "r", encoding="utf-8") as exported_file:
            lines = exported_file.readlines()

        first_pose_translation = None
        first_pose_rotation = None
        first_triangle_position = None
        first_triangle_positions = []
        frame_zero_poses = {}
        in_skeleton = False
        in_triangles = False
        pending_material = False
        current_time = None

        for raw_line in lines:
            stripped = raw_line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            if stripped == "skeleton":
                in_skeleton = True
                continue
            if in_skeleton:
                if stripped == "end":
                    in_skeleton = False
                    current_time = None
                    continue
                if stripped.startswith("time "):
                    current_time = int(stripped.split()[1])
                    continue
                parts = stripped.split()
                if len(parts) >= 7:
                    bone_index = int(parts[0])
                    translation = [float(parts[1]), float(parts[2]), float(parts[3])]
                    rotation = [float(parts[4]), float(parts[5]), float(parts[6])]
                    if current_time == 0:
                        frame_zero_poses[bone_index] = {
                            "translation": translation,
                            "rotation": rotation,
                        }
                        if first_pose_translation is None:
                            first_pose_translation = translation
                            first_pose_rotation = rotation

        for raw_line in lines:
            stripped = raw_line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            if stripped == "triangles":
                in_triangles = True
                pending_material = True
                continue
            if not in_triangles:
                continue
            if stripped == "end":
                break
            if pending_material:
                pending_material = False
                continue

            parts = stripped.split()
            if len(parts) >= 9:
                triangle_position = [float(parts[1]), float(parts[2]), float(parts[3])]
                if first_triangle_position is None:
                    first_triangle_position = triangle_position
                if len(first_triangle_positions) < 3:
                    first_triangle_positions.append(triangle_position)
                if len(first_triangle_positions) == 3:
                    break

        return {
            "first_pose_translation": first_pose_translation,
            "first_pose_rotation": first_pose_rotation,
            "first_triangle_position": first_triangle_position,
            "first_triangle_positions": first_triangle_positions,
            "frame_zero_poses": frame_zero_poses,
        }

    def validate_export_transform_gate(self, exported_path, case_name, format_name, export_variant_name, export_options):
        expectation = self.ctx.config.get_case_expectation("export_transform_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        if expectation["format"] != format_name or export_variant_name != "text":
            return

        normalized_expected_options = expectation["export_options"].replace(" ", "")
        normalized_actual_options = (export_options or "").replace(" ", "")
        if normalized_actual_options != normalized_expected_options:
            return

        if format_name == "dmx":
            parsed = self.parse_exported_dmx_transform_gate_data(exported_path)
            if parsed["up_axis"] != expectation["expected_up_axis"]:
                raise RuntimeError(
                    f"Export transform gate failed for {normalized_case_name}. "
                    f"expected upAxis={expectation['expected_up_axis']} got {parsed['up_axis']}"
                )
            self.assert_close_triplet(
                parsed["transform_position"],
                expectation["expected_transform_position"],
                f"Export transform gate failed for {normalized_case_name} transform position",
            )
            if len(parsed["bind_positions"]) < len(expectation["expected_bind_positions"]):
                raise RuntimeError(
                    f"Export transform gate failed for {normalized_case_name}. "
                    f"insufficient bind positions: {parsed['bind_positions']}"
                )
            for index, expected_position in enumerate(expectation["expected_bind_positions"]):
                self.assert_close_triplet(
                    parsed["bind_positions"][index],
                    expected_position,
                    f"Export transform gate failed for {normalized_case_name} bind position {index}",
                )
            return

        if format_name == "smd":
            parsed = self.parse_exported_smd_transform_gate_data(exported_path)
            self.assert_close_triplet(
                parsed["first_pose_translation"],
                expectation["expected_first_pose_translation"],
                f"Export transform gate failed for {normalized_case_name} first pose",
            )
            self.assert_close_triplet(
                parsed["first_triangle_position"],
                expectation["expected_first_triangle_position"],
                f"Export transform gate failed for {normalized_case_name} first triangle",
            )

    def validate_animation_layer_export_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("animation_layer_export_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])
        format_name = self.ctx.detect_format(base_input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)
        cmds = self.ctx.cmds

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(base_input_path, **self.ctx.build_import_kwargs(format_config, expectation["base_import_options"]))
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"Animation layer export gate failed for {normalized_case_name}. base import produced no DAG roots"
            )

        for layer_import in expectation["layer_imports"]:
            layer_input_path = self.ctx.resolve_input_path(layer_import["case"])
            cmds.file(layer_input_path, **self.ctx.build_import_kwargs(format_config, layer_import["options"]))
            if not cmds.objExists(layer_import["layer_name"]):
                raise RuntimeError(
                    f"Animation layer export gate failed for {normalized_case_name}. "
                    f"missing expected layer after setup: {layer_import['layer_name']}"
                )

        reference_values = self.ctx.sample_plugs(expectation["compare_plugs"], expectation["sample_times"])
        reference_animations = SnapshotUtils.snapshot_selected_animation_bindings(cmds, expectation["compare_plugs"])
        if not reference_animations:
            raise RuntimeError(
                f"Animation layer export gate failed for {normalized_case_name}. layered scene produced no animation bindings"
            )

        export_variant_name = expectation.get("export_variant_name", "text")
        export_variant = next(
            variant for variant in format_config.export_variants if variant.name == export_variant_name
        )
        export_extension = export_variant.extension
        exported_path = os.path.join(
            self.ctx.output_dir,
            f"{self.ctx.make_case_output_name(case_name)}.animation_layer_export{export_extension}",
        )

        if imported_roots:
            cmds.select(imported_roots, replace=True)
        else:
            cmds.select(clear=True)
        cmds.file(rename=exported_path)
        cmds.file(**self.ctx.build_export_kwargs(format_config, export_variant))

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(
            exported_path,
            **self.ctx.build_import_kwargs(format_config, expectation.get("reimport_options", expectation["base_import_options"])),
        )
        reimported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not reimported_roots:
            raise RuntimeError(
                f"Animation layer export gate failed for {normalized_case_name}. reimport produced no DAG roots"
            )

        candidate_values = self.ctx.sample_plugs(expectation["compare_plugs"], expectation["sample_times"])
        candidate_animations = SnapshotUtils.snapshot_selected_animation_bindings(cmds, expectation["compare_plugs"])
        if not candidate_animations:
            raise RuntimeError(
                f"Animation layer export gate failed for {normalized_case_name}. exported file reimport produced no animation bindings"
            )

        for plug_name in expectation["compare_plugs"]:
            for reference_value, candidate_value, time_value in zip(
                reference_values[plug_name],
                candidate_values[plug_name],
                expectation["sample_times"],
            ):
                if abs(reference_value - candidate_value) > 1.0e-4:
                    raise RuntimeError(
                        f"Animation layer export gate failed for {normalized_case_name}. "
                        f"value mismatch on {plug_name} at time {time_value}: reference={reference_value} candidate={candidate_value}"
                    )

    def validate_smd_selected_export_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("smd_selected_export_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        input_format_name = self.ctx.detect_format(input_path)
        input_format_config = self.ctx.config.get_format_config(input_format_name)
        smd_format_config = self.ctx.config.get_format_config("smd")
        cmds = self.ctx.cmds

        self.ctx.ensure_plugins_loaded([
            self.ctx.plugin_paths_by_format[input_format_name],
            self.ctx.plugin_paths_by_format["smd"],
        ])

        cmds.file(new=True, force=True)
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **self.ctx.build_import_kwargs(input_format_config))
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"SMD selected export gate failed for {normalized_case_name}. import produced no DAG roots"
            )

        cmds.select(imported_roots, replace=True)
        plain_exported_path = os.path.join(
            self.ctx.output_dir,
            f"{self.ctx.make_case_output_name(case_name)}.selected_export_plain.smd",
        )
        corrected_exported_path = os.path.join(
            self.ctx.output_dir,
            f"{self.ctx.make_case_output_name(case_name)}.selected_export_corrected.smd",
        )

        cmds.file(rename=plain_exported_path)
        cmds.file(
            force=True,
            exportSelected=True,
            type=smd_format_config.export_type,
            options=expectation["export_options"],
        )
        plain_parsed = self.parse_exported_smd_transform_gate_data(plain_exported_path)

        cmds.file(rename=corrected_exported_path)
        cmds.file(
            force=True,
            exportSelected=True,
            type=smd_format_config.export_type,
            options=expectation["corrected_export_options"],
        )
        corrected_parsed = self.parse_exported_smd_transform_gate_data(corrected_exported_path)

        if plain_parsed["first_triangle_position"] is None or corrected_parsed["first_triangle_position"] is None:
            raise RuntimeError(
                f"SMD selected export gate failed for {normalized_case_name}. exported file was missing triangles"
            )

        self.assert_close_triplet(
            plain_parsed["first_triangle_position"],
            expectation["expected_first_triangle_position"],
            f"SMD selected export gate failed for {normalized_case_name} first triangle",
        )

        changed_bones = []
        all_bone_indices = sorted(set(plain_parsed["frame_zero_poses"]) | set(corrected_parsed["frame_zero_poses"]))
        for bone_index in all_bone_indices:
            if plain_parsed["frame_zero_poses"].get(bone_index) != corrected_parsed["frame_zero_poses"].get(bone_index):
                changed_bones.append(bone_index)

        if changed_bones != expectation["expected_changed_bones"]:
            raise RuntimeError(
                f"SMD selected export gate failed for {normalized_case_name}. "
                f"expected changed bones {expectation['expected_changed_bones']} got {changed_bones}"
            )

        corrected_pose = corrected_parsed["frame_zero_poses"].get(expectation["expected_changed_bones"][0])
        if not corrected_pose:
            raise RuntimeError(
                f"SMD selected export gate failed for {normalized_case_name}. "
                f"missing corrected pose for bone {expectation['expected_changed_bones'][0]}"
            )

        self.assert_close_triplet(
            corrected_pose["rotation"],
            expectation["expected_corrected_first_pose_rotation"],
            f"SMD selected export gate failed for {normalized_case_name} corrected first pose rotation",
        )

        expected_corrected_triangle_positions = expectation.get("expected_corrected_triangle_positions")
        if expected_corrected_triangle_positions:
            if len(corrected_parsed["first_triangle_positions"]) < len(expected_corrected_triangle_positions):
                raise RuntimeError(
                    f"SMD selected export gate failed for {normalized_case_name}. "
                    f"insufficient corrected triangle positions: {corrected_parsed['first_triangle_positions']}"
                )
            for index, expected_position in enumerate(expected_corrected_triangle_positions):
                self.assert_close_triplet(
                    corrected_parsed["first_triangle_positions"][index],
                    expected_position,
                    f"SMD selected export gate failed for {normalized_case_name} corrected triangle position {index}",
                )

    def validate_dmx_selected_export_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("dmx_selected_export_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        input_format_name = self.ctx.detect_format(input_path)
        input_format_config = self.ctx.config.get_format_config(input_format_name)
        dmx_format_config = self.ctx.config.get_format_config("dmx")
        cmds = self.ctx.cmds

        self.ctx.ensure_plugins_loaded([
            self.ctx.plugin_paths_by_format[input_format_name],
            self.ctx.plugin_paths_by_format["dmx"],
        ])

        cmds.file(new=True, force=True)
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **self.ctx.build_import_kwargs(input_format_config))
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"DMX selected export gate failed for {normalized_case_name}. import produced no DAG roots"
            )

        cmds.select(imported_roots, replace=True)
        plain_exported_path = os.path.join(
            self.ctx.output_dir,
            f"{self.ctx.make_case_output_name(case_name)}.selected_export_plain.dmx",
        )
        corrected_exported_path = os.path.join(
            self.ctx.output_dir,
            f"{self.ctx.make_case_output_name(case_name)}.selected_export_corrected.dmx",
        )

        cmds.file(rename=plain_exported_path)
        cmds.file(
            force=True,
            exportSelected=True,
            type=dmx_format_config.export_type,
            options=expectation["export_options"],
        )
        plain_parsed = self.parse_exported_dmx_transform_gate_data(plain_exported_path)

        cmds.file(rename=corrected_exported_path)
        cmds.file(
            force=True,
            exportSelected=True,
            type=dmx_format_config.export_type,
            options=expectation["corrected_export_options"],
        )
        corrected_parsed = self.parse_exported_dmx_transform_gate_data(corrected_exported_path)

        if len(corrected_parsed["bind_positions"]) < len(expectation["expected_corrected_bind_positions"]):
            raise RuntimeError(
                f"DMX selected export gate failed for {normalized_case_name}. "
                f"insufficient corrected bind positions: {corrected_parsed['bind_positions']}"
            )

        for index, expected_position in enumerate(expectation["expected_corrected_bind_positions"]):
            self.assert_close_triplet(
                corrected_parsed["bind_positions"][index],
                expected_position,
                f"DMX selected export gate failed for {normalized_case_name} corrected bind position {index}",
            )

        changed_transform_names = []
        all_transform_names = sorted(
            set(plain_parsed["transform_data_by_name"]) | set(corrected_parsed["transform_data_by_name"])
        )
        for transform_name in all_transform_names:
            if plain_parsed["transform_data_by_name"].get(transform_name) != corrected_parsed["transform_data_by_name"].get(transform_name):
                changed_transform_names.append(transform_name)

        if changed_transform_names != expectation["expected_changed_transform_names"]:
            raise RuntimeError(
                f"DMX selected export gate failed for {normalized_case_name}. "
                f"expected changed transforms {expectation['expected_changed_transform_names']} got {changed_transform_names}"
            )

        corrected_transform = corrected_parsed["transform_data_by_name"].get(
            expectation["expected_changed_transform_names"][0]
        )
        if not corrected_transform:
            raise RuntimeError(
                f"DMX selected export gate failed for {normalized_case_name}. "
                f"missing corrected transform for {expectation['expected_changed_transform_names'][0]}"
            )

        corrected_orientation = corrected_transform.get("orientation")
        if corrected_orientation is None:
            raise RuntimeError(
                f"DMX selected export gate failed for {normalized_case_name}. "
                f"missing corrected orientation for {expectation['expected_changed_transform_names'][0]}"
            )

        for actual_value, expected_value in zip(corrected_orientation, expectation["expected_corrected_orientation"]):
            if abs(actual_value - expected_value) > 1.0e-3:
                raise RuntimeError(
                    f"DMX selected export gate failed for {normalized_case_name} corrected orientation: "
                    f"actual={corrected_orientation} expected={expectation['expected_corrected_orientation']}"
                )

    def validate_selected_export_scene_transform_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("selected_export_scene_transform_gate_expectations", case_name)
        if not expectation:
            return

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        export_variant = next(variant for variant in format_config.export_variants if variant.name == "text")
        cmds = self.ctx.cmds

        self.ctx.ensure_plugins_loaded([self.ctx.plugin_paths_by_format[format_name]])

        cmds.file(new=True, force=True)
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **self.ctx.build_import_kwargs(format_config))
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        if not imported_roots:
            raise RuntimeError(
                f"Selected export scene transform gate failed for {normalized_case_name}. import produced no DAG roots"
            )

        baseline_node_frames = {}
        for node_suffix in expectation.get("sample_nodes", []):
            node_path = self.resolve_unique_node_by_suffix(cmds, node_suffix)
            baseline_node_frames[node_suffix] = self.sample_node_world_frame(cmds, node_path)

        baseline_mesh_positions = {}
        for mesh_spec in expectation.get("sample_mesh_vertices", []):
            mesh_path = self.resolve_unique_mesh_by_suffix(cmds, mesh_spec["mesh_suffix"])
            baseline_mesh_positions[mesh_spec["mesh_suffix"]] = self.sample_mesh_vertex_positions(
                cmds,
                mesh_path,
                mesh_spec["indices"],
            )

        baseline_mesh_normals = {}
        for mesh_spec in expectation.get("sample_mesh_normals", []):
            mesh_path = self.resolve_unique_mesh_by_suffix(cmds, mesh_spec["mesh_suffix"])
            baseline_mesh_normals[mesh_spec["mesh_suffix"]] = self.sample_mesh_vertex_normals(
                cmds,
                mesh_path,
                mesh_spec["indices"],
            )

        for matrix_variant in expectation["matrix_variants"]:
            correction_matrix = self.build_correction_matrix(matrix_variant["options"])

            cmds.file(new=True, force=True)
            before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
            cmds.file(input_path, **self.ctx.build_import_kwargs(format_config))
            imported_roots = self.ctx.collect_imported_roots(before_assemblies)
            if imported_roots:
                cmds.select(imported_roots, replace=True)
            else:
                cmds.select(clear=True)

            exported_path = os.path.join(
                self.ctx.output_dir,
                f"{self.ctx.make_case_output_name(case_name)}.scene_transform_gate.{matrix_variant['name']}{export_variant.extension}",
            )
            cmds.file(rename=exported_path)
            cmds.file(
                force=True,
                exportSelected=True,
                type=format_config.export_type,
                options=matrix_variant["options"],
            )

            cmds.file(new=True, force=True)
            before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
            cmds.file(exported_path, **self.ctx.build_import_kwargs(format_config))
            reimported_roots = self.ctx.collect_imported_roots(before_assemblies)
            if not reimported_roots:
                raise RuntimeError(
                    f"Selected export scene transform gate failed for {normalized_case_name}. "
                    f"reimport produced no DAG roots for matrix {matrix_variant['name']}"
                )

            for node_suffix, baseline_frame in baseline_node_frames.items():
                candidate_frame = self.sample_node_world_frame(
                    cmds,
                    self.resolve_unique_node_by_suffix(cmds, node_suffix),
                )
                sample_names = ["origin"] if not matrix_variant.get("compare_axes", False) else list(baseline_frame.keys())
                for sample_name in sample_names:
                    baseline_triplet = baseline_frame[sample_name]
                    expected_triplet = self.apply_matrix_to_triplet(correction_matrix, baseline_triplet)
                    self.assert_close_triplet(
                        candidate_frame[sample_name],
                        expected_triplet,
                        (
                            f"Selected export scene transform gate failed for {normalized_case_name} "
                            f"node={node_suffix} sample={sample_name} matrix={matrix_variant['name']}"
                        ),
                        tolerance=2.0e-3,
                    )

            for mesh_suffix, baseline_vertices in baseline_mesh_positions.items():
                candidate_vertices = self.sample_mesh_vertex_positions(
                    cmds,
                    self.resolve_unique_mesh_by_suffix(cmds, mesh_suffix),
                    sorted(baseline_vertices.keys()),
                )
                for vertex_index, baseline_triplet in baseline_vertices.items():
                    expected_triplet = self.apply_matrix_to_triplet(correction_matrix, baseline_triplet)
                    self.assert_close_triplet(
                        candidate_vertices[vertex_index],
                        expected_triplet,
                        (
                            f"Selected export scene transform gate failed for {normalized_case_name} "
                            f"mesh={mesh_suffix} vertex={vertex_index} matrix={matrix_variant['name']}"
                        ),
                        tolerance=2.0e-3,
                    )

            for mesh_suffix, baseline_normals in baseline_mesh_normals.items():
                candidate_normals = self.sample_mesh_vertex_normals(
                    cmds,
                    self.resolve_unique_mesh_by_suffix(cmds, mesh_suffix),
                    sorted(baseline_normals.keys()),
                )
                for vertex_index, baseline_triplet in baseline_normals.items():
                    expected_triplet = self.apply_matrix_to_direction_triplet(correction_matrix, baseline_triplet)
                    self.assert_close_triplet(
                        candidate_normals[vertex_index],
                        expected_triplet,
                        (
                            f"Selected export scene transform gate failed for {normalized_case_name} "
                            f"mesh={mesh_suffix} normalVertex={vertex_index} matrix={matrix_variant['name']}"
                        ),
                        tolerance=2.0e-3,
                    )

    def validate_vta_import_gate(self, case_name):
        expectation = self.ctx.config.get_case_expectation("vta_import_gate_expectations", case_name)
        if not expectation:
            return False

        normalized_case_name = self.ctx.config.normalize_case_name(case_name)
        plugin_path = self.ctx.plugin_paths_by_format.get("smd")
        if not plugin_path:
            raise RuntimeError(f"Missing SMD plugin while running VTA case '{case_name}'")

        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])

        base_input_path = self.ctx.resolve_input_path(expectation["base_case"])
        base_import_kwargs = dict(
            i=True,
            type="Valve SMD Import",
            ignoreVersion=True,
            mergeNamespacesOnClash=False,
            options=expectation["base_import_options"],
        )
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(base_input_path, **base_import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        selectable_root = None
        for root_path in imported_roots:
            if expectation["root_name_substring"] in root_path:
                selectable_root = root_path
                break
        if not selectable_root:
            raise RuntimeError(f"VTA import gate failed for {normalized_case_name}: could not resolve imported root")

        cmds.select(selectable_root, replace=True)
        vta_input_path = self.ctx.resolve_input_path(case_name)
        cmds.file(
            vta_input_path,
            i=True,
            type="Valve VTA Import",
            ignoreVersion=True,
            mergeNamespacesOnClash=False,
            options=expectation["vta_import_options"],
        )

        visible_meshes = [
            mesh for mesh in (cmds.ls(type="mesh", long=True) or [])
            if not cmds.getAttr(mesh + ".intermediateObject")
        ]
        mesh_to_target_count = {}
        for mesh_path in visible_meshes:
            history = cmds.listHistory(mesh_path, pruneDagObjects=True) or []
            blendshapes = [node for node in history if cmds.nodeType(node) == "blendShape"]
            if not blendshapes:
                continue
            mesh_to_target_count[mesh_path] = len(cmds.listAttr(blendshapes[0] + ".w", multi=True) or [])

        for requirement in expectation["expected_blendshape_targets"]:
            matching_meshes = [
                mesh_path for mesh_path in mesh_to_target_count
                if mesh_path.endswith(requirement["mesh_suffix"])
            ]
            if len(matching_meshes) != 1:
                raise RuntimeError(
                    f"VTA import gate failed for {normalized_case_name}: expected one mesh ending with "
                    f"{requirement['mesh_suffix']} got {matching_meshes}"
                )
            actual_target_count = mesh_to_target_count[matching_meshes[0]]
            if actual_target_count != requirement["target_count"]:
                raise RuntimeError(
                    f"VTA import gate failed for {normalized_case_name}: mesh {matching_meshes[0]} "
                    f"expected {requirement['target_count']} targets got {actual_target_count}"
                )

        for mesh_suffix in expectation.get("expected_mesh_without_blendshape", []):
            matching_meshes = [mesh_path for mesh_path in visible_meshes if mesh_path.endswith(mesh_suffix)]
            if len(matching_meshes) != 1:
                raise RuntimeError(
                    f"VTA import gate failed for {normalized_case_name}: expected one mesh ending with "
                    f"{mesh_suffix} got {matching_meshes}"
                )
            if matching_meshes[0] in mesh_to_target_count:
                raise RuntimeError(
                    f"VTA import gate failed for {normalized_case_name}: mesh {matching_meshes[0]} "
                    f"should not receive a blendShape target set"
                )

        self.ctx.write_marker(
            os.path.join(self.ctx.output_dir, f"{self.ctx.make_case_output_name(case_name)}.vta_import_gate.txt")
        )
        return True


class RegressionRunner:
    def __init__(self, cmds, config, plugin_paths_by_format, sample_dir, output_dir):
        self.ctx = MayaRegressionContext(cmds, config, plugin_paths_by_format, sample_dir, output_dir)
        self.validator = GateValidator(self.ctx)

    def verify_roundtrip(
        self,
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
        cmds = self.ctx.cmds
        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded(plugin_paths)
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        import_kwargs = dict(i=True, type=import_type, ignoreVersion=True, ra=True, mergeNamespacesOnClash=False, defaultNamespace=True)
        if import_options:
            import_kwargs["options"] = import_options
        cmds.file(exported_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)
        candidate_meshes = SnapshotUtils.snapshot_scene_meshes()
        candidate_node_types = SnapshotUtils.snapshot_imported_node_types(cmds, imported_roots)
        candidate_skins = SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots)
        candidate_blendshapes = SnapshotUtils.snapshot_blendshape_bindings(cmds, imported_roots)
        candidate_animations = SnapshotUtils.snapshot_animation_bindings(cmds, imported_roots)

        SnapshotComparator.compare_mesh_snapshots(reference_meshes, candidate_meshes)
        SnapshotComparator.compare_node_type_snapshots(reference_node_types, candidate_node_types)
        SnapshotComparator.compare_skin_snapshots(reference_skins, candidate_skins)
        SnapshotComparator.compare_blendshape_snapshots(reference_blendshapes, candidate_blendshapes)
        SnapshotComparator.compare_animation_snapshots(reference_animations, candidate_animations)

        self.ctx.write_marker(mesh_marker_path)
        self.ctx.write_marker(type_marker_path)
        self.ctx.write_marker(skin_marker_path)
        self.ctx.write_marker(blendshape_marker_path)
        self.ctx.write_marker(animation_marker_path)

    def run_case(self, case_name, import_options=""):
        label = case_name if not import_options else f"{case_name} [{import_options}]"
        sys.stdout.write(f"[maya_dmx_case] {label}\n")
        sys.stdout.flush()

        if not import_options and self.validator.validate_vta_import_gate(case_name):
            return

        input_path = self.ctx.resolve_input_path(case_name)
        format_name = self.ctx.detect_format(input_path)
        format_config = self.ctx.config.get_format_config(format_name)
        plugin_path = self.ctx.ensure_plugin_loaded_for_format(format_name)

        case_output_name = self.ctx.make_case_output_name(case_name)
        options_suffix = ""
        if import_options:
            safe_options = import_options.replace("=", "").replace(";", "_").replace(" ", "")
            options_suffix = f".{safe_options}"

        import_animation_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.import_animgate.txt")
        append_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.append_gate.txt")
        update_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.update_gate.txt")
        topology_update_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.topology_update_gate.txt")
        skin_influence_update_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.skin_influence_update_gate.txt")
        skin_cluster_reuse_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.skin_cluster_reuse_gate.txt")
        animation_layer_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.animation_layer_gate.txt")
        animation_layer_replace_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.animation_layer_replace_gate.txt")
        animation_layer_multi_new_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.animation_layer_multi_new_gate.txt")
        animation_layer_export_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.animation_layer_export_gate.txt")
        source_delta_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.source_delta_gate.txt")
        smd_selected_export_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.smd_selected_export_gate.txt")
        dmx_selected_export_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.dmx_selected_export_gate.txt")
        scene_transform_gate_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.scene_transform_gate.txt")

        import_kwargs = self.ctx.build_import_kwargs(format_config, import_options)
        cmds = self.ctx.cmds

        cmds.file(new=True, force=True)
        self.ctx.ensure_plugins_loaded([plugin_path])
        before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
        cmds.file(input_path, **import_kwargs)
        imported_roots = self.ctx.collect_imported_roots(before_assemblies)

        original_meshes = SnapshotUtils.snapshot_scene_meshes()
        original_node_types = SnapshotUtils.snapshot_imported_node_types(cmds, imported_roots)
        original_skins = SnapshotUtils.snapshot_skin_bindings(cmds, imported_roots)
        original_blendshapes = SnapshotUtils.snapshot_blendshape_bindings(cmds, imported_roots)
        original_animations = SnapshotUtils.snapshot_animation_bindings(cmds, imported_roots)
        self.validator.validate_animation_gate(case_name, original_animations)
        self.ctx.write_marker(import_animation_gate_marker)

        for export_variant in format_config.export_variants:
            cmds.file(new=True, force=True)
            self.ctx.ensure_plugins_loaded([plugin_path])
            before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
            cmds.file(input_path, **import_kwargs)
            imported_roots = self.ctx.collect_imported_roots(before_assemblies)

            exported_path = os.path.join(
                self.ctx.output_dir,
                f"{case_output_name}{options_suffix}.maya_export{export_variant.extension}",
            )
            mesh_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.{export_variant.mesh_marker_suffix}")
            type_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.{export_variant.type_marker_suffix}")
            skin_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.{export_variant.skin_marker_suffix}")
            blendshape_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.{export_variant.blendshape_marker_suffix}")
            animation_marker = os.path.join(self.ctx.output_dir, f"{case_output_name}{options_suffix}.{export_variant.animation_marker_suffix}")

            if imported_roots:
                cmds.select(imported_roots, replace=True)
            else:
                cmds.select(clear=True)
            cmds.file(rename=exported_path)
            cmds.file(**self.ctx.build_export_kwargs(format_config, export_variant))

            self.validator.validate_export_transform_gate(
                exported_path,
                case_name,
                format_name,
                export_variant.name,
                export_variant.options,
            )

            self.verify_roundtrip(
                [plugin_path],
                format_config.import_type,
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
            self.validator.validate_append_gate(case_name, format_config, input_path)
            self.ctx.write_marker(append_gate_marker)

            self.validator.validate_update_gate(case_name, format_config, input_path)
            self.ctx.write_marker(update_gate_marker)

            self.validator.validate_topology_update_gate(case_name, format_config, input_path)
            self.ctx.write_marker(topology_update_gate_marker)

            self.validator.validate_paired_update_gate(case_name)
            self.validator.validate_skin_influence_update_gate(case_name, format_config, input_path)
            self.ctx.write_marker(skin_influence_update_gate_marker)

            self.validator.validate_skin_cluster_reuse_gate(case_name, format_config, input_path)
            self.ctx.write_marker(skin_cluster_reuse_gate_marker)

            self.validator.validate_animation_layer_import_gate(case_name)
            self.ctx.write_marker(animation_layer_gate_marker)

            self.validator.validate_animation_layer_replace_reimport_gate(case_name)
            self.ctx.write_marker(animation_layer_replace_gate_marker)

            self.validator.validate_animation_layer_new_layers_gate(case_name)
            self.ctx.write_marker(animation_layer_multi_new_gate_marker)

            self.validator.validate_animation_layer_export_gate(case_name)
            self.ctx.write_marker(animation_layer_export_gate_marker)

            self.validator.validate_source_delta_import_gate(case_name)
            self.ctx.write_marker(source_delta_gate_marker)

            self.validator.validate_dmx_selected_export_gate(case_name)
            self.ctx.write_marker(dmx_selected_export_gate_marker)

            self.validator.validate_smd_selected_export_gate(case_name)
            self.ctx.write_marker(smd_selected_export_gate_marker)

            self.validator.validate_selected_export_scene_transform_gate(case_name)
            self.ctx.write_marker(scene_transform_gate_marker)

        if format_name == "dmx" and original_skins and not import_options:
            self.run_case(case_name, import_options="applyAxisCorrection=0")

        transform_gate_expectation = self.ctx.config.get_case_expectation("export_transform_gate_expectations", case_name)
        if transform_gate_expectation and not import_options:
            transform_gate_extension = ".dmx" if format_name == "dmx" else ".smd"
            cmds.file(new=True, force=True)
            self.ctx.ensure_plugins_loaded([plugin_path])
            before_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
            cmds.file(input_path, **import_kwargs)
            imported_roots = self.ctx.collect_imported_roots(before_assemblies)
            if imported_roots:
                cmds.select(imported_roots, replace=True)
            else:
                cmds.select(clear=True)

            transform_exported_path = os.path.join(
                self.ctx.output_dir,
                f"{case_output_name}.transform_gate{transform_gate_extension}",
            )
            cmds.file(rename=transform_exported_path)
            cmds.file(
                force=True,
                exportSelected=True,
                type=format_config.export_type,
                options=transform_gate_expectation["export_options"],
            )
            self.validator.validate_export_transform_gate(
                transform_exported_path,
                case_name,
                format_name,
                "text",
                transform_gate_expectation["export_options"],
            )

    def run_cases(self, case_names):
        for case_name in case_names:
            self.run_case(case_name)
