#!/usr/bin/env python3
"""Replace only the head in baked skeleton meshes, without rebuilding the game.

Authoring dependencies: numpy, scikit-image, zstandard. Run after bpat_baker:
  python scripts/sculpt-skeleton-skull.py assets/creatures build/bin/assets/creatures

The body, bind-space coordinates, bone IDs and animation assets are preserved.
UV (-5, occlusion) is interpreted by character_shading.glsl.
"""

import argparse
import struct
from pathlib import Path

import numpy as np
import zstandard
from skimage.measure import marching_cubes

HEADER = struct.Struct("<4s5I2Q")
VERTEX = np.dtype(
    [
        ("position", "<f4", 3),
        ("normal", "<f4", 3),
        ("uv", "<f4", 2),
        ("bones", "u1", 4),
        ("weights", "<f4", 4),
        ("role", "u1"),
        ("padding", "u1", 3),
    ]
)
HEAD_BONE = 5
SKIN_ROLE = 2


def ellipsoid(p, center, radius, power=2):
    q = np.abs((p - center) / radius)
    return (np.sum(q**power, axis=-1) ** (1 / power) - 1) * min(radius)


def union(a, b, blend=0.08):
    h = np.maximum(blend - np.abs(a - b), 0) / blend
    return np.minimum(a, b) - h * h * blend * 0.25


def sockets(p):
    # Slightly angular orbits, tilted toward the nasal bridge, cut into bone.
    q = p.copy()
    q[..., 1] += (np.abs(q[..., 0]) - 0.29) * 0.18
    return ellipsoid(q, (0.29, 0.13, 0.67), (0.225, 0.19, 0.31), 2.5), ellipsoid(
        q, (-0.29, 0.13, 0.67), (0.225, 0.19, 0.31), 2.5
    )


def nose(p):
    # Narrow bridge and a wider, rounded triangular nasal opening below it.
    width = np.clip(0.055 - p[..., 1] * 0.28, 0.04, 0.12)
    return (
        np.sqrt(
            (p[..., 0] / width) ** 2
            + ((p[..., 1] + 0.14) / 0.16) ** 2
            + ((p[..., 2] - 0.70) / 0.27) ** 2
        )
        - 1
    ) * 0.09


def skull_field(p):
    cranium = ellipsoid(p, (0, 0.26, -0.09), (0.61, 0.62, 0.64))
    face = ellipsoid(p, (0, -0.025, 0.34), (0.535, 0.40, 0.37))
    skull = union(cranium, face, 0.14)
    # Cheek arches belong to the skull surface, not separate round cheek pads.
    for side in (-1, 1):
        cheek = ellipsoid(p, (side * 0.46, -0.14, 0.39), (0.14, 0.14, 0.26))
        skull = union(skull, cheek)
    skull = np.maximum(skull, -0.335 - p[..., 1])
    for cavity in (*sockets(p), nose(p)):
        skull = np.maximum(skull, -cavity)
    return skull


def jaw_field(p):
    outer = ellipsoid(p, (0, -0.555, 0.24), (0.435, 0.10, 0.40), 2.5)
    inner = ellipsoid(p, (0, -0.545, 0.21), (0.335, 0.22, 0.30), 2.5)
    jaw = np.maximum(outer, -inner)
    jaw = np.maximum(jaw, -p[..., 2] - 0.035)
    for side in (-1, 1):
        ramus = ellipsoid(p, (side * 0.40, -0.355, 0.055), (0.07, 0.245, 0.105))
        jaw = union(jaw, ramus, 0.05)
    return jaw


def surface(field, low, high, step):
    axes = [np.arange(a, b + step, step) for a, b in zip(low, high, strict=True)]
    grid = np.stack(np.meshgrid(*axes, indexing="ij"), axis=-1)
    values = field(grid)
    positions, faces, _, _ = marching_cubes(
        values, 0, spacing=(step, step, step), allow_degenerate=False
    )
    positions += low
    # Compute outward normals from the field; winding is checked independently.
    delta = 0.001
    normals = np.stack(
        [
            field(positions + np.eye(3)[i] * delta)
            - field(positions - np.eye(3)[i] * delta)
            for i in range(3)
        ],
        axis=-1,
    )
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-10)
    geometric = np.cross(
        positions[faces[:, 1]] - positions[faces[:, 0]],
        positions[faces[:, 2]] - positions[faces[:, 0]],
    )
    inward = np.sum(geometric * normals[faces].mean(axis=1), axis=1) < 0
    faces[inward] = faces[inward, ::-1]
    return positions, normals, faces


def make_head(minimal):
    parts = []
    step = 0.052 if minimal else 0.028
    for field in (skull_field, jaw_field):
        parts.append(surface(field, (-0.75, -0.72, -0.8), (0.75, 0.96, 0.85), step))
    # Individual short crowns follow the dental arch; no mouth plate or fangs.
    for lower in (False, True):
        for angle in np.linspace(-1.12, 1.12, 8):
            center = np.array(
                [0.315 * np.sin(angle), -0.475 if lower else -0.36,
                 0.285 + 0.325 * np.cos(angle)]
            )
            radius = np.array([0.039, 0.051 if lower else 0.063, 0.055])

            def tooth(p, center=center, radius=radius):
                return ellipsoid(p, center, radius, 4)

            parts.append(surface(tooth, center - radius * 1.3,
                                 center + radius * 1.3, 0.021 if minimal else 0.014))
    vertices, indices = [], []
    count = 0
    for part_index, (p, normals, faces) in enumerate(parts):
        v = np.zeros(len(p), dtype=VERTEX)
        # Skeleton bind head: HEAD_RADIUS after skeleton proportions is .1968.
        v["position"] = p * 0.1968 + (0, 1.645, 0.008)
        v["normal"] = normals
        v["bones"][:, 0] = HEAD_BONE
        v["weights"][:, 0] = 1
        v["role"] = SKIN_ROLE
        v["uv"][:, 0] = -5
        tone = np.ones(len(p))
        if part_index == 0:
            for cavity in sockets(p):
                interior = np.clip((0.66 - p[:, 2]) / 0.22, 0, 1)
                rim = np.clip((0.035 - np.abs(cavity)) / 0.035, 0, 1)
                tone = np.minimum(tone, 1 - interior * rim * 0.93)
            interior = np.clip((0.68 - p[:, 2]) / 0.17, 0, 1)
            rim = np.clip((0.025 - np.abs(nose(p))) / 0.025, 0, 1)
            tone = np.minimum(tone, 1 - interior * rim * 0.94)
        v["uv"][:, 1] = tone
        vertices.append(v)
        indices.append(faces.astype("<u4") + count)
        count += len(v)
    return np.concatenate(vertices), np.concatenate(indices)


def replace_head(path, head):
    data = path.read_bytes()
    if data[:4] != b"BPRM":
        data = zstandard.ZstdDecompressor().decompress(data)
    magic, version, lod, nv, ni, reserved, io, vo = HEADER.unpack_from(data)
    if magic != b"BPRM" or version != 1 or ni % 3:
        raise ValueError(f"{path}: unsupported BPRM header")
    vertices = np.frombuffer(data, VERTEX, nv, vo)
    faces = np.frombuffer(data, "<u4", ni, io).reshape(-1, 3)
    if np.max(faces) >= nv:
        raise ValueError(f"{path}: invalid vertex index")
    is_head = vertices["bones"][:, 0] == HEAD_BONE
    if not is_head.any():
        raise ValueError(f"{path}: no head vertices")
    if np.any(is_head[faces].any(axis=1) & ~is_head[faces].all(axis=1)):
        raise ValueError(f"{path}: head shares triangles with body")
    body = vertices[~is_head]
    remap = np.cumsum(~is_head) - 1
    body_faces = remap[faces[~is_head[faces].any(axis=1)]]
    new_vertices = np.concatenate((body, head[0]))
    new_faces = np.concatenate((body_faces, head[1] + len(body))).astype("<u4")
    index_offset = 48
    vertex_offset = (index_offset + new_faces.nbytes + 15) // 16 * 16
    output = bytearray(vertex_offset)
    HEADER.pack_into(output, 0, magic, version, lod, len(new_vertices),
                     new_faces.size, reserved, index_offset, vertex_offset)
    output[index_offset:index_offset + new_faces.nbytes] = new_faces.tobytes()
    output.extend(new_vertices.tobytes())
    path.write_bytes(zstandard.ZstdCompressor(level=19).compress(output))
    print(f"{path}: preserved {len(body)} body vertices; installed {len(head[0])} skull vertices")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_dirs", nargs="+", type=Path)
    args = parser.parse_args()
    for lod in ("full", "minimal"):
        head = make_head(lod == "minimal")
        for directory in args.asset_dirs:
            replace_head(directory / f"skeleton_humanoid_{lod}.bprm", head)


if __name__ == "__main__":
    main()
