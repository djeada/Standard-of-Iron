#!/usr/bin/env python3
"""Replace a creature's authored eye patches with proper eyes on the skin.

The elephant export carries its eyes as a pair of large flat wedges half sunk
into the head. Whatever colour they are painted they read as gashes rather than
eyes, and no amount of nudging fixes a shape that was never eye-shaped.

This regenerates them: for each side, the existing patch tells us roughly where
the eye belongs, we find the actual skin surface there, and we lay a small disc
against it, oriented to the surface and standing slightly proud. Skinning is
inherited from the patch being replaced, so the new eye follows the head exactly
as the old one did.

Geometry work belongs offline, so this runs at asset build time and the result
is committed - the renderer never sees the unrepaired mesh.

Usage:
    author_creature_eyes.py PACKAGE --detail MATERIAL --body MATERIAL \
        [--radius R] [--segments N] [--margin M] [--min-z Z]
"""

from __future__ import annotations

import argparse
import math
import pathlib

from complete_creature_mirror import (
    find_primitive,
    load_buffer,
    read_accessor,
    read_package,
    repack,
    write_package,
)
from creature_mesh_geometry import (
    closest_point_on_triangle,
    cross,
    dot,
    islands,
    normalize,
    sub,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=pathlib.Path)
    parser.add_argument("--detail", required=True, help="material holding the eyes")
    parser.add_argument("--body", required=True, help="material they sit on")
    parser.add_argument("--radius", type=float, default=0.11, help="eye radius")
    parser.add_argument("--segments", type=int, default=8, help="rim vertices")
    parser.add_argument(
        "--margin", type=float, default=0.03, help="how far proud of the skin"
    )
    parser.add_argument(
        "--target",
        type=lambda v: tuple(float(x) for x in v.split(",")),
        default=None,
        help="x,y,z in authoring space where the eye should sit; x is taken as "
        "a distance from the lateral plane and mirrored for the far side",
    )
    parser.add_argument(
        "--lateral-plane",
        type=float,
        default=0.0,
        help="x plane separating the two sides of the body",
    )
    parser.add_argument(
        "--min-z",
        type=float,
        default=0.0,
        help="only islands beyond this depth are eyes; the rest of the material "
        "(a tail tuft, say) is carried through untouched",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    document = read_package(args.package)
    buffer = load_buffer(document)

    detail, _ = find_primitive(document, args.detail)
    body, _ = find_primitive(document, args.body)

    attributes = detail["attributes"]
    positions = read_accessor(document, buffer, attributes["POSITION"])
    normals = read_accessor(document, buffer, attributes["NORMAL"])
    joints = read_accessor(document, buffer, attributes["JOINTS_0"])
    weights = read_accessor(document, buffer, attributes["WEIGHTS_0"])
    indices = [v[0] for v in read_accessor(document, buffer, detail["indices"])]

    body_positions = read_accessor(document, buffer, body["attributes"]["POSITION"])
    body_indices = [v[0] for v in read_accessor(document, buffer, body["indices"])]

    eye_islands = []
    kept_islands = []
    for island in islands(len(positions), indices):
        centre = [
            sum(positions[v][axis] for v in island) / len(island) for axis in range(3)
        ]
        (eye_islands if centre[2] >= args.min_z else kept_islands).append(
            (island, centre)
        )
    if not eye_islands:
        raise SystemExit(f"no '{args.detail}' island lies beyond z={args.min_z}")

    sides: dict[int, list] = {}
    for island, centre in eye_islands:
        side = 1 if centre[0] >= args.lateral_plane else -1
        sides.setdefault(side, []).append((island, centre))
    if len(sides) != 2:
        raise SystemExit("expected eye patches on both sides of the body")

    keep = sorted({v for island, _ in kept_islands for v in island})
    remap = {old: new for new, old in enumerate(keep)}
    new_positions = [positions[v] for v in keep]
    new_normals = [normals[v] for v in keep]
    new_joints = [joints[v] for v in keep]
    new_weights = [weights[v] for v in keep]
    new_indices = []
    for start in range(0, len(indices) - 2, 3):
        tri = [indices[start + k] for k in range(3)]
        if all(v in remap for v in tri):
            new_indices += [remap[v] for v in tri]

    for side, group in sorted(sides.items()):
        anchor = [
            sum(c[axis] * len(i) for i, c in group) / sum(len(i) for i, _ in group)
            for axis in range(3)
        ]
        outward = (float(side), 0.0, 0.0)

        if args.target is not None:
            target = [
                args.lateral_plane + side * abs(args.target[0]),
                args.target[1],
                args.target[2],
            ]
        else:
            target = anchor

        best = None
        for start in range(0, len(body_indices) - 2, 3):
            tri = [body_positions[body_indices[start + k]] for k in range(3)]
            if all((v[0] - args.lateral_plane) * side <= 0.0 for v in tri):
                continue
            point = closest_point_on_triangle(target, tri)
            offset = sub(point, target)
            distance = dot(offset, offset)
            if best is None or distance < best[0]:
                normal = normalize(cross(sub(tri[1], tri[0]), sub(tri[2], tri[0])))
                best = (distance, point, normal)
        if best is None:
            raise SystemExit(f"no body surface on side {side}")
        _, centre_on_skin, face_normal = best

        if dot(face_normal, outward) < 0.0:
            face_normal = (-face_normal[0], -face_normal[1], -face_normal[2])

        face_normal = normalize(
            tuple(face_normal[axis] + outward[axis] for axis in range(3))
        )

        up = (0.0, 1.0, 0.0)
        if abs(dot(face_normal, up)) > 0.9:
            up = (0.0, 0.0, 1.0)
        tangent = normalize(cross(up, face_normal))
        bitangent = normalize(cross(face_normal, tangent))

        seed = group[0][0][0]
        centre_vertex = len(new_positions)
        origin = [
            centre_on_skin[axis] + face_normal[axis] * args.margin for axis in range(3)
        ]
        new_positions.append(tuple(origin))
        new_normals.append(face_normal)
        new_joints.append(joints[seed])
        new_weights.append(weights[seed])

        rim_start = len(new_positions)
        for step in range(args.segments):
            angle = 2.0 * math.pi * step / args.segments
            offset = [
                tangent[axis] * math.cos(angle) * args.radius
                + bitangent[axis] * math.sin(angle) * args.radius
                for axis in range(3)
            ]

            rim = [
                origin[axis] + offset[axis] - face_normal[axis] * args.margin * 0.6
                for axis in range(3)
            ]
            new_positions.append(tuple(rim))
            new_normals.append(
                normalize(
                    tuple(
                        face_normal[axis] * 1.4 + offset[axis] / args.radius
                        for axis in range(3)
                    )
                )
            )
            new_joints.append(joints[seed])
            new_weights.append(weights[seed])

        for step in range(args.segments):
            a = rim_start + step
            b = rim_start + (step + 1) % args.segments

            if side > 0:
                new_indices += [centre_vertex, a, b]
            else:
                new_indices += [centre_vertex, b, a]

    print(
        f"{args.package}: replaced {sum(len(i) for i, _ in eye_islands)} patch "
        f"vertices with {len(sides)} eyes of {args.segments} segments "
        f"(radius {args.radius})"
    )
    print(
        f"  '{args.detail}' now has {len(new_positions)} vertices, "
        f"{len(new_indices) // 3} triangles"
    )

    repack(
        document,
        buffer,
        {
            attributes["POSITION"]: new_positions,
            attributes["NORMAL"]: new_normals,
            attributes["JOINTS_0"]: new_joints,
            attributes["WEIGHTS_0"]: new_weights,
            detail["indices"]: [(v,) for v in new_indices],
        },
    )
    write_package(args.package, document)

    total_vertices = 0
    total_triangles = 0
    verify = read_package(args.package)
    for mesh in verify["meshes"]:
        for prim in mesh["primitives"]:
            total_vertices += verify["accessors"][prim["attributes"]["POSITION"]][
                "count"
            ]
            total_triangles += verify["accessors"][prim["indices"]]["count"] // 3
    print(
        f"{args.package}: package now {total_vertices} vertices, "
        f"{total_triangles} triangles"
    )


if __name__ == "__main__":
    main()
