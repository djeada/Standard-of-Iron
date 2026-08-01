#!/usr/bin/env python3
"""Complete a compiled creature package by mirroring geometry it is missing.

Production creature meshes must arrive at the runtime already complete: the
renderer is not allowed to build, mirror or otherwise repair authored geometry
while the game is running. When an authored model reaches us with one half of a
symmetric part missing, the repair belongs here, in an offline step, and the
completed package is what gets committed.

The operation is a no-op when every triangle already has a mirror twin, so it is
safe to re-run against an already completed package.

Usage:
    complete_creature_mirror.py PACKAGE --material MATERIAL_NAME --plane X
    complete_creature_mirror.py PACKAGE --material MATERIAL_NAME --plane X --check
"""

from __future__ import annotations

import argparse
import base64
import json
import math
import pathlib
import struct
import zlib

MAGIC = b"SOICM001"

COMPONENT_FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
COMPONENT_SIZES = {"b": 1, "B": 1, "h": 2, "H": 2, "I": 4, "f": 4}
TYPE_COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=pathlib.Path)
    parser.add_argument(
        "--material",
        required=True,
        help="name of the material whose primitive should be completed",
    )
    parser.add_argument(
        "--plane",
        type=float,
        required=True,
        help="x coordinate of the mirror plane in authoring space",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=1.0e-3,
        help="distance below which two vertices count as the same point",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="report what would change and exit without writing",
    )
    return parser.parse_args()


def read_package(path: pathlib.Path) -> dict:
    package = path.read_bytes()
    if not package.startswith(MAGIC):
        raise SystemExit(f"{path}: not a creature package")
    return json.loads(zlib.decompress(package[len(MAGIC) + 4 :]))


def write_package(path: pathlib.Path, document: dict) -> None:
    payload = json.dumps(
        document, ensure_ascii=True, separators=(",", ":"), sort_keys=True
    ).encode("utf-8")
    path.write_bytes(
        MAGIC + struct.pack(">I", len(payload)) + zlib.compress(payload, 9)
    )


def load_buffer(document: dict) -> bytearray:
    uri = document["buffers"][0]["uri"]
    return bytearray(base64.b64decode(uri.split(",", 1)[1]))


def read_accessor(document: dict, buffer: bytes, index: int) -> list[tuple]:
    accessor = document["accessors"][index]
    if "sparse" in accessor:
        raise SystemExit("sparse accessors are not supported")
    view = document["bufferViews"][accessor["bufferView"]]
    fmt = COMPONENT_FORMATS[accessor["componentType"]]
    components = TYPE_COMPONENTS[accessor["type"]]
    element = COMPONENT_SIZES[fmt] * components
    stride = view.get("byteStride") or element
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [
        struct.unpack_from("<" + fmt * components, buffer, base + i * stride)
        for i in range(accessor["count"])
    ]


def node_matrix(node: dict) -> list[float]:
    if "matrix" in node:
        m = node["matrix"]
        return [m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13],
                m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]]
    tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
    x, y, z, w = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    sx, sy, sz = node.get("scale", (1.0, 1.0, 1.0))
    rotation = [
        1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
        2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
        2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y),
    ]
    scale = (sx, sy, sz)
    return [
        rotation[0] * scale[0], rotation[1] * scale[1], rotation[2] * scale[2], tx,
        rotation[3] * scale[0], rotation[4] * scale[1], rotation[5] * scale[2], ty,
        rotation[6] * scale[0], rotation[7] * scale[1], rotation[8] * scale[2], tz,
        0.0, 0.0, 0.0, 1.0,
    ]


def multiply(a: list[float], b: list[float]) -> list[float]:
    return [
        sum(a[row * 4 + k] * b[k * 4 + col] for k in range(4))
        for row in range(4)
        for col in range(4)
    ]


def sided_joints(document: dict, tolerance: float) -> set[int]:
    """Joints that belong to a left/right pair.

    Two joints pair up when they rest at the same height and depth but at
    different widths, which is what a mirrored limb looks like whatever plane
    the rig happens to be built around. Centreline joints - spine, trunk, tail -
    have no such partner and can be reused by mirrored geometry unchanged.
    """
    rest = joint_rest_positions(document)
    sided = set()
    for index, point in enumerate(rest):
        for other, candidate in enumerate(rest):
            if other == index:
                continue
            if (
                abs(point[1] - candidate[1]) <= tolerance
                and abs(point[2] - candidate[2]) <= tolerance
                and abs(point[0] - candidate[0]) > tolerance
            ):
                sided.add(index)
                break
    return sided


def joint_rest_positions(document: dict) -> list[tuple[float, float, float]]:
    nodes = document["nodes"]
    parents: dict[int, int] = {}
    for index, node in enumerate(nodes):
        for child in node.get("children", ()):
            parents[child] = index

    world: dict[int, list[float]] = {}

    def resolve(index: int) -> list[float]:
        if index not in world:
            local = node_matrix(nodes[index])
            parent = parents.get(index)
            world[index] = local if parent is None else multiply(resolve(parent), local)
        return world[index]

    return [
        (resolve(joint)[3], resolve(joint)[7], resolve(joint)[11])
        for joint in document["skins"][0]["joints"]
    ]


def find_primitive(document: dict, material_name: str) -> tuple[dict, int]:
    materials = document.get("materials", [])
    matches = [i for i, m in enumerate(materials) if m.get("name") == material_name]
    if len(matches) != 1:
        known = ", ".join(sorted(m.get("name", "?") for m in materials))
        raise SystemExit(f"material '{material_name}' is not unique; known: {known}")
    material = matches[0]
    primitives = [
        p for m in document["meshes"] for p in m["primitives"]
        if p.get("material") == material
    ]
    if len(primitives) != 1:
        raise SystemExit(f"material '{material_name}' is not used by exactly one primitive")
    return primitives[0], material


def missing_mirror_triangles(
    positions: list[tuple], indices: list[int], plane: float, tolerance: float
) -> list[int]:
    """Return the first index of every triangle that has no mirror twin."""

    # Bucketing coordinates would let a pair straddling a bucket edge look
    # unmatched and get duplicated, so resolve each point to a canonical vertex
    # id by explicit proximity search first.
    def near(a: tuple, b: tuple) -> bool:
        return all(abs(a[axis] - b[axis]) <= tolerance for axis in range(3))

    representatives: list[int] = []
    canonical: list[int] = []
    for index, point in enumerate(positions):
        for rep in representatives:
            if near(point, positions[rep]):
                canonical.append(rep)
                break
        else:
            representatives.append(index)
            canonical.append(index)

    def mirror_of(point: tuple[float, float, float]) -> int | None:
        target = (2.0 * plane - point[0], point[1], point[2])
        return next(
            (rep for rep in representatives if near(target, positions[rep])), None
        )

    present = {
        frozenset(canonical[indices[start + offset]] for offset in range(3))
        for start in range(0, len(indices) - 2, 3)
    }

    missing = []
    for start in range(0, len(indices) - 2, 3):
        twins = [
            mirror_of(positions[indices[start + offset]]) for offset in range(3)
        ]
        if any(twin is None for twin in twins) or frozenset(twins) not in present:
            missing.append(start)
    return missing


def repack(document: dict, buffer: bytes, overrides: dict[int, list[tuple]]) -> None:
    """Rewrite every accessor tightly, applying the supplied replacements."""
    payload = bytearray()
    views = []
    accessors = []
    for index, accessor in enumerate(document["accessors"]):
        values = overrides.get(index) or read_accessor(document, buffer, index)
        fmt = COMPONENT_FORMATS[accessor["componentType"]]
        components = TYPE_COMPONENTS[accessor["type"]]
        offset = len(payload)
        for value in values:
            payload += struct.pack("<" + fmt * components, *value)
        while len(payload) % 4 != 0:
            payload += b"\0"
        view = {
            "buffer": 0,
            "byteLength": len(payload) - offset,
            "byteOffset": offset,
        }
        source_view = document["bufferViews"][accessor["bufferView"]]
        if "target" in source_view:
            view["target"] = source_view["target"]
        views.append(view)

        rebuilt = {k: v for k, v in accessor.items() if k not in ("byteOffset", "min", "max")}
        rebuilt["bufferView"] = index
        rebuilt["count"] = len(values)
        if "min" in accessor and "max" in accessor:
            rebuilt["min"] = [min(v[c] for v in values) for c in range(components)]
            rebuilt["max"] = [max(v[c] for v in values) for c in range(components)]
        accessors.append(rebuilt)

    document["bufferViews"] = views
    document["accessors"] = accessors
    document["buffers"] = [
        {
            "byteLength": len(payload),
            "uri": "data:application/octet-stream;base64,"
            + base64.b64encode(bytes(payload)).decode("ascii"),
        }
    ]


def main() -> None:
    args = parse_args()
    document = read_package(args.package)
    buffer = load_buffer(document)

    primitive, _ = find_primitive(document, args.material)
    attributes = primitive["attributes"]
    position_index = attributes["POSITION"]
    normal_index = attributes["NORMAL"]
    joints_index = attributes["JOINTS_0"]
    weights_index = attributes["WEIGHTS_0"]
    indices_index = primitive["indices"]

    positions = read_accessor(document, buffer, position_index)
    normals = read_accessor(document, buffer, normal_index)
    joints = read_accessor(document, buffer, joints_index)
    weights = read_accessor(document, buffer, weights_index)
    indices = [value[0] for value in read_accessor(document, buffer, indices_index)]

    missing = missing_mirror_triangles(positions, indices, args.plane, args.tolerance)
    if not missing:
        print(f"{args.package}: '{args.material}' is already mirror-complete")
        return

    # A mirrored vertex keeps the bones it was skinned to, which is only correct
    # for centreline bones. Geometry hanging off a left/right bone would need
    # that bone swapped for its opposite, and guessing at that mapping is how
    # you end up with an eye that follows the wrong ear.
    sided = sided_joints(document, args.tolerance)
    offenders = {
        joint
        for start in missing
        for offset in range(3)
        for joint, weight in zip(
            joints[indices[start + offset]], weights[indices[start + offset]]
        )
        if weight > 0.0 and joint in sided
    }
    if offenders:
        names = [document["nodes"][document["skins"][0]["joints"][j]].get("name", str(j))
                 for j in sorted(offenders)]
        raise SystemExit(
            "refusing to mirror geometry skinned to left/right bones "
            f"{names}; author the missing half in the source model"
        )

    remapped: dict[int, int] = {}
    new_positions = list(positions)
    new_normals = list(normals)
    new_joints = list(joints)
    new_weights = list(weights)
    new_indices = list(indices)

    def mirror_vertex(source: int) -> int:
        if source not in remapped:
            x, y, z = positions[source]
            nx, ny, nz = normals[source]
            remapped[source] = len(new_positions)
            new_positions.append((2.0 * args.plane - x, y, z))
            new_normals.append((-nx, ny, nz))
            new_joints.append(joints[source])
            new_weights.append(weights[source])
        return remapped[source]

    for start in missing:
        a, b, c = (indices[start + offset] for offset in range(3))
        new_indices += [mirror_vertex(a), mirror_vertex(c), mirror_vertex(b)]

    print(
        f"{args.package}: mirroring {len(missing)} triangle(s) of '{args.material}' "
        f"about x={args.plane} -> {len(new_positions)} vertices, "
        f"{len(new_indices) // 3} triangles"
    )
    if args.check:
        return

    repack(
        document,
        buffer,
        {
            position_index: new_positions,
            normal_index: new_normals,
            joints_index: new_joints,
            weights_index: new_weights,
            indices_index: [(value,) for value in new_indices],
        },
    )
    write_package(args.package, document)

    verify = read_package(args.package)
    verify_buffer = load_buffer(verify)
    verify_primitive, _ = find_primitive(verify, args.material)
    left = missing_mirror_triangles(
        read_accessor(verify, verify_buffer, verify_primitive["attributes"]["POSITION"]),
        [v[0] for v in read_accessor(verify, verify_buffer, verify_primitive["indices"])],
        args.plane,
        args.tolerance,
    )
    if left:
        raise SystemExit(f"completion failed: {len(left)} triangle(s) still unmirrored")
    total_vertices = 0
    total_triangles = 0
    for mesh in verify["meshes"]:
        for prim in mesh["primitives"]:
            total_vertices += verify["accessors"][prim["attributes"]["POSITION"]]["count"]
            total_triangles += verify["accessors"][prim["indices"]]["count"] // 3
    digest = zlib.crc32(args.package.read_bytes())
    print(
        f"{args.package}: wrote {total_vertices} vertices, {total_triangles} triangles "
        f"(crc32 {digest:08x})"
    )


if __name__ == "__main__":
    main()
