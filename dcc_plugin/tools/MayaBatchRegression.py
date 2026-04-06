import argparse
import os
import sys

import maya.standalone


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


def verify_roundtrip(cmds, plugin_path, exported_path, reference_meshes, marker_path):
    cmds.file(new=True, force=True)
    if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
        cmds.loadPlugin(plugin_path)

    cmds.file(exported_path, i=True, type="Valve DMX Import", ignoreVersion=True, ra=True, mergeNamespacesOnClash=False)
    candidate_meshes = snapshot_scene_meshes()
    compare_mesh_snapshots(reference_meshes, candidate_meshes)

    with open(marker_path, "w", encoding="utf-8") as marker_file:
        marker_file.write("ok\n")


def collect_imported_roots(cmds, before_assemblies):
    after_assemblies = set(cmds.ls(assemblies=True, long=True) or [])
    imported_roots = sorted(after_assemblies - before_assemblies)
    return imported_roots


def make_case_output_name(case_name):
    return case_name.replace("\\", "__").replace("/", "__")


def run_case(cmds, plugin_path, sample_dir, output_dir, case_name):
    sys.stdout.write(f"[maya_dmx_case] {case_name}\n")
    sys.stdout.flush()

    input_path = os.path.join(sample_dir, f"{case_name}.dmx")
    case_output_name = make_case_output_name(case_name)
    exported_text = os.path.join(output_dir, f"{case_output_name}.maya_export.dmx")
    exported_binary = os.path.join(output_dir, f"{case_output_name}.maya_export.dmxb")
    roundtrip_text_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_text_meshcheck.txt")
    roundtrip_binary_marker = os.path.join(output_dir, f"{case_output_name}.roundtrip_binary_meshcheck.txt")

    if not os.path.isfile(input_path):
        raise RuntimeError(f"Missing sample file: {input_path}")

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
    cmds.file(rename=exported_text)
    cmds.file(force=True, exportSelected=True, type="Valve DMX Export")

    cmds.file(rename=exported_binary)
    cmds.file(force=True, options="binary=1", exportSelected=True, type="Valve DMX Export")

    verify_roundtrip(cmds, plugin_path, exported_text, original_meshes, roundtrip_text_marker)
    verify_roundtrip(cmds, plugin_path, exported_binary, original_meshes, roundtrip_binary_marker)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--samples", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cases", nargs="+", required=True)
    args = parser.parse_args()

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
