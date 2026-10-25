#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

import maya.api.OpenMaya as om
import maya.api.OpenMayaAnim as oma
import maya.cmds as cmds
import maya.standalone


def _matrix16(values):
    return [float(v) for v in values]


def _vector3(attr):
    try:
        values = cmds.getAttr(attr)[0]
        return [float(values[0]), float(values[1]), float(values[2])]
    except Exception:
        return None


def _joint_dump(joint):
    parent = cmds.listRelatives(joint, parent=True, fullPath=True) or []
    children = cmds.listRelatives(joint, children=True, fullPath=True, type="joint") or []
    return {
        "name": joint,
        "parent": parent[0] if parent else None,
        "children": children,
        "worldMatrix": _matrix16(cmds.xform(joint, q=True, ws=True, matrix=True)),
        "translate": _vector3(f"{joint}.translate"),
        "rotate": _vector3(f"{joint}.rotate"),
        "scale": _vector3(f"{joint}.scale"),
        "jointOrient": _vector3(f"{joint}.jointOrient"),
        "rotateAxis": _vector3(f"{joint}.rotateAxis"),
        "segmentScaleCompensate": bool(cmds.getAttr(f"{joint}.segmentScaleCompensate"))
        if cmds.objExists(f"{joint}.segmentScaleCompensate")
        else None,
        "rotateOrder": int(cmds.getAttr(f"{joint}.rotateOrder"))
        if cmds.objExists(f"{joint}.rotateOrder")
        else None,
    }


def _skin_dump(skin_cluster):
    sel = om.MSelectionList()
    sel.add(skin_cluster)
    skin_obj = sel.getDependNode(0)
    skin_fn = oma.MFnSkinCluster(skin_obj)

    influences = skin_fn.influenceObjects()
    influence_names = [inf.fullPathName() for inf in influences]

    geometry_count = skin_fn.numOutputConnections()
    geometries = []
    for geom_index in range(geometry_count):
        geom_path = skin_fn.getPathAtIndex(geom_index)
        mesh_fn = om.MFnMesh(geom_path)
        num_verts = mesh_fn.numVertices
        comp_fn = om.MFnSingleIndexedComponent()
        component = comp_fn.create(om.MFn.kMeshVertComponent)
        comp_fn.addElements(list(range(num_verts)))
        weights, influence_count = skin_fn.getWeights(geom_path, component)

        vertex_weights = []
        for v in range(num_verts):
            row = []
            base = v * influence_count
            for i in range(influence_count):
                w = float(weights[base + i])
                if abs(w) > 1.0e-8:
                    row.append({"influence": influence_names[i], "weight": w})
            vertex_weights.append(row)

        geometries.append(
            {
                "path": geom_path.fullPathName(),
                "vertexCount": num_verts,
                "influenceCount": influence_count,
                "weights": vertex_weights,
            }
        )

    return {
        "name": skin_cluster,
        "influences": influence_names,
        "geometries": geometries,
    }


def _dump_scene(scene_path: Path, plugin_path: Path, out_dir: Path, translator: str) -> int:
    out_dir.mkdir(parents=True, exist_ok=True)
    imported_scene = out_dir / "scene_imported.ma"
    dump_json = out_dir / "scene_dump.json"
    report_txt = out_dir / "scene_dump.txt"

    cmds.file(new=True, force=True)
    cmds.loadPlugin(str(plugin_path), quiet=True)
    cmds.file(str(scene_path), i=True, type=translator, ignoreVersion=True, force=True)

    cmds.file(rename=str(imported_scene))
    cmds.file(save=True, type="mayaAscii")

    joints = cmds.ls(type="joint", long=True) or []
    skin_clusters = cmds.ls(type="skinCluster", long=True) or []

    data = {
        "scene": str(scene_path),
        "translator": translator,
        "joints": [_joint_dump(j) for j in joints],
        "skins": [_skin_dump(sc) for sc in skin_clusters],
    }

    dump_json.write_text(json.dumps(data, indent=2), encoding="utf-8")

    lines = []
    lines.append(f"scene: {scene_path}")
    lines.append(f"translator: {translator}")
    lines.append(f"joints: {len(joints)}")
    for joint in joints:
        world = cmds.xform(joint, q=True, ws=True, matrix=True)
        lines.append(f"[joint] {joint}")
        lines.append("  worldMatrix=" + " ".join(f"{v:.6f}" for v in world))
        lines.append("  translate=" + str(_vector3(f"{joint}.translate")))
        lines.append("  rotate=" + str(_vector3(f"{joint}.rotate")))
        lines.append("  scale=" + str(_vector3(f"{joint}.scale")))
        lines.append("  jointOrient=" + str(_vector3(f"{joint}.jointOrient")))
        lines.append("  rotateAxis=" + str(_vector3(f"{joint}.rotateAxis")))
        if cmds.objExists(f"{joint}.segmentScaleCompensate"):
            lines.append(f"  segmentScaleCompensate={bool(cmds.getAttr(f'{joint}.segmentScaleCompensate'))}")
    lines.append(f"skinClusters: {len(skin_clusters)}")
    for skin in skin_clusters:
        lines.append(f"[skinCluster] {skin}")
    report_txt.write_text("\n".join(lines), encoding="utf-8")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--scene",
        default=r"D:\_Code_Here\Git\csgo-src\dcc_plugin\samples\gltf\xiaoyu_tekken_3d_model\scene.gltf",
    )
    parser.add_argument(
        "--plugin",
        default=r"D:\_Code_Here\Git\csgo-src\dcc_plugin\maya_module\plug-ins\windows\2022\maya_gltf.mll",
    )
    parser.add_argument(
        "--out",
        default=r"D:\_Code_Here\Git\csgo-src\dcc_plugin\build\gltf_import_dump",
    )
    parser.add_argument(
        "--translator",
        default="glTF/GLB Import (cgltf)",
    )
    args = parser.parse_args()

    maya.standalone.initialize(name="python")
    try:
        return _dump_scene(
            Path(args.scene),
            Path(args.plugin),
            Path(args.out),
            args.translator,
        )
    finally:
        maya.standalone.uninitialize()


if __name__ == "__main__":
    raise SystemExit(main())
