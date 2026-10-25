#!/usr/bin/env python3
"""Generate binary buffer files for glTF test samples."""
import struct
import os

SAMPLES_DIR = os.path.join(os.path.dirname(__file__), "..", "samples", "gltf")


def write_triangle_bin():
    """Generate simple_triangle.bin: 3 vertices forming a right triangle."""
    # Positions (3 float xyz): right triangle in XY plane
    vertices = [
        (0.0, 0.0, 0.0),  # v0: origin
        (1.0, 0.0, 0.0),  # v1: +X
        (0.0, 1.0, 0.0),  # v2: +Y
    ]
    # Indices (unsigned short)
    indices = [0, 1, 2]

    data = b""
    for v in vertices:
        data += struct.pack("<fff", *v)
    for idx in indices:
        data += struct.pack("<H", idx)

    path = os.path.join(SAMPLES_DIR, "simple_triangle.bin")
    with open(path, "wb") as f:
        f.write(data)
    print(f"Wrote {path} ({len(data)} bytes)")


def write_box_bin():
    """Generate simple_box.bin: A unit cube with normals and UVs."""
    # 24 vertices (4 per face * 6 faces), with normals and UVs
    # Face layout: +X, -X, +Y, -Y, +Z, -Z

    # Positions for a unit cube ([-0.5, 0.5] range)
    # Each face uses 4 unique vertices for proper normal/UV mapping
    h = 0.5
    cube_positions = [
        # +X face (right)
        (h, -h, -h), (h, -h, h), (h, h, -h), (h, h, h),
        # -X face (left)
        (-h, -h, h), (-h, -h, -h), (-h, h, h), (-h, h, -h),
        # +Y face (top)
        (-h, h, -h), (h, h, -h), (-h, h, h), (h, h, h),
        # -Y face (bottom)
        (-h, -h, -h), (h, -h, -h), (-h, -h, h), (h, -h, h),
        # +Z face (front)
        (-h, -h, h), (h, -h, h), (-h, h, h), (h, h, h),
        # -Z face (back)
        (h, -h, -h), (-h, -h, -h), (h, h, -h), (-h, h, -h),
    ]

    cube_normals = [
        (1, 0, 0), (1, 0, 0), (1, 0, 0), (1, 0, 0),
        (-1, 0, 0), (-1, 0, 0), (-1, 0, 0), (-1, 0, 0),
        (0, 1, 0), (0, 1, 0), (0, 1, 0), (0, 1, 0),
        (0, -1, 0), (0, -1, 0), (0, -1, 0), (0, -1, 0),
        (0, 0, 1), (0, 0, 1), (0, 0, 1), (0, 0, 1),
        (0, 0, -1), (0, 0, -1), (0, 0, -1), (0, 0, -1),
    ]

    cube_uvs = [
        (0, 0), (1, 0), (0, 1), (1, 1),
        (0, 0), (1, 0), (0, 1), (1, 1),
        (0, 0), (1, 0), (0, 1), (1, 1),
        (0, 0), (1, 0), (0, 1), (1, 1),
        (0, 0), (1, 0), (0, 1), (1, 1),
        (0, 0), (1, 0), (0, 1), (1, 1),
    ]

    # Indices (2 triangles per face, 6 faces = 36 indices)
    cube_indices = []
    for base in range(6):
        i = base * 4
        cube_indices.extend([i, i+1, i+2, i+2, i+1, i+3])

    data = b""
    for pos, nrm, uv in zip(cube_positions, cube_normals, cube_uvs):
        data += struct.pack("<fff", *pos)
    for nrm in cube_normals:
        data += struct.pack("<fff", *nrm)
    for uv in cube_uvs:
        data += struct.pack("<ff", *uv)
    for idx in cube_indices:
        data += struct.pack("<H", idx)

    path = os.path.join(SAMPLES_DIR, "simple_box.bin")
    with open(path, "wb") as f:
        f.write(data)
    print(f"Wrote {path} ({len(data)} bytes)")


def write_simple_mesh_bin():
    """Generate simple_mesh.bin: a quad as 2 triangles (4 verts, 6 indices)."""
    vertices = [
        (0.0, 0.0, 0.0),  # v0
        (1.0, 0.0, 0.0),  # v1
        (0.0, 1.0, 0.0),  # v2
        (1.0, 1.0, 0.0),  # v3
    ]
    normals = [
        (0, 0, 1), (0, 0, 1), (0, 0, 1), (0, 0, 1),
    ]
    uvs = [
        (0, 0), (1, 0), (0, 1), (1, 1),
    ]
    indices = [0, 1, 2, 2, 1, 3]

    data = b""
    for v in vertices:
        data += struct.pack("<fff", *v)
    for n in normals:
        data += struct.pack("<fff", *n)
    for uv in uvs:
        data += struct.pack("<ff", *uv)
    for idx in indices:
        data += struct.pack("<H", idx)

    path = os.path.join(SAMPLES_DIR, "simple_mesh.bin")
    with open(path, "wb") as f:
        f.write(data)
    print(f"Wrote {path} ({len(data)} bytes)")


def write_simple_mesh_gltf():
    """Generate simple_mesh.gltf: a quad (2 triangles) with normals and UVs."""
    gltf = {
        "asset": {"version": "2.0", "generator": "test"},
        "scene": 0,
        "scenes": [{"name": "Scene", "nodes": [0]}],
        "nodes": [{"name": "quad", "mesh": 0}],
        "meshes": [{
            "name": "quad_mesh",
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "NORMAL": 1,
                    "TEXCOORD_0": 2
                },
                "indices": 3
            }]
        }],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3", "max": [1.0, 1.0, 0.0], "min": [0.0, 0.0, 0.0]},
            {"bufferView": 0, "byteOffset": 48, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 96, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 1, "byteOffset": 0, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 128, "target": 34962, "byteStride": 12},
            {"buffer": 0, "byteOffset": 128, "byteLength": 12, "target": 34963},
        ],
        "buffers": [
            {"byteLength": 144, "uri": "simple_mesh.bin"}
        ]
    }

    import json
    path = os.path.join(SAMPLES_DIR, "simple_mesh.gltf")
    with open(path, "w") as f:
        json.dump(gltf, f, indent=2)
    print(f"Wrote {path}")


def write_multi_node_gltf():
    """Generate simple_hierarchy.gltf: a simple scene with a root and child mesh."""
    import json

    # Positions: a triangle in XY plane
    vertices = [
        (0.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
    ]
    indices = [0, 1, 2]

    data = b""
    for v in vertices:
        data += struct.pack("<fff", *v)
    for idx in indices:
        data += struct.pack("<H", idx)

    bin_path = os.path.join(SAMPLES_DIR, "simple_hierarchy.bin")
    with open(bin_path, "wb") as f:
        f.write(data)

    gltf = {
        "asset": {"version": "2.0", "generator": "test"},
        "scene": 0,
        "scenes": [{"name": "Scene", "nodes": [0]}],
        "nodes": [
            {
                "name": "Root",
                "children": [1],
                "translation": [0.0, 0.0, 0.0]
            },
            {
                "name": "ChildMesh",
                "mesh": 0,
                "translation": [2.0, 0.0, 0.0]
            }
        ],
        "meshes": [{
            "name": "triangle_mesh",
            "primitives": [{
                "attributes": {"POSITION": 0},
                "indices": 1
            }]
        }],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "max": [1.0, 1.0, 0.0], "min": [0.0, 0.0, 0.0]},
            {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
            {"buffer": 0, "byteOffset": 36, "byteLength": 6, "target": 34963},
        ],
        "buffers": [
            {"byteLength": 44, "uri": "simple_hierarchy.bin"}
        ]
    }

    path = os.path.join(SAMPLES_DIR, "simple_hierarchy.gltf")
    with open(path, "w") as f:
        json.dump(gltf, f, indent=2)
    print(f"Wrote {path}")


if __name__ == "__main__":
    os.makedirs(SAMPLES_DIR, exist_ok=True)
    write_triangle_bin()
    write_box_bin()
    write_simple_mesh_bin()
    write_simple_mesh_gltf()
    write_multi_node_gltf()
    print("Done.")
