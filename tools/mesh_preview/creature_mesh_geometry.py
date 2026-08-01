"""Small geometry helpers shared by the offline creature mesh tools.

Kept deliberately dependency-free: these tools run from a plain interpreter as
part of the asset build, with no numpy available.
"""

from __future__ import annotations

import math


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def normalize(v):
    length = math.sqrt(dot(v, v))
    return (v[0] / length, v[1] / length, v[2] / length) if length > 1e-12 else v


def ray_hits(origin, direction, positions, indices) -> list[float]:
    """Forward distances at which the ray crosses the mesh (Moller-Trumbore)."""
    out = []
    for start in range(0, len(indices) - 2, 3):
        v0 = positions[indices[start]]
        v1 = positions[indices[start + 1]]
        v2 = positions[indices[start + 2]]
        edge1 = sub(v1, v0)
        edge2 = sub(v2, v0)
        pvec = cross(direction, edge2)
        det = dot(edge1, pvec)
        if abs(det) < 1e-12:
            continue
        inv = 1.0 / det
        tvec = sub(origin, v0)
        u = inv * dot(tvec, pvec)
        if u < 0.0 or u > 1.0:
            continue
        qvec = cross(tvec, edge1)
        v = inv * dot(direction, qvec)
        if v < 0.0 or u + v > 1.0:
            continue
        distance = inv * dot(edge2, qvec)
        if distance > 1e-6:
            out.append(distance)
    return out


def islands(vertex_count: int, indices: list[int]) -> list[list[int]]:
    """Connected components of a triangle soup, as lists of vertex indices."""
    parent = list(range(vertex_count))

    def find(a: int) -> int:
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a: int, b: int) -> None:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for start in range(0, len(indices) - 2, 3):
        union(indices[start], indices[start + 1])
        union(indices[start + 1], indices[start + 2])

    groups: dict[int, list[int]] = {}
    for vertex in range(vertex_count):
        groups.setdefault(find(vertex), []).append(vertex)
    return list(groups.values())


def closest_point_on_triangle(point, tri):
    """Nearest point of a triangle to an arbitrary point."""
    a, b, c = tri
    ab = sub(b, a)
    ac = sub(c, a)
    ap = sub(point, a)
    d1 = dot(ab, ap)
    d2 = dot(ac, ap)
    if d1 <= 0.0 and d2 <= 0.0:
        return a
    bp = sub(point, b)
    d3 = dot(ab, bp)
    d4 = dot(ac, bp)
    if d3 >= 0.0 and d4 <= d3:
        return b
    vc = d1 * d4 - d3 * d2
    if vc <= 0.0 and d1 >= 0.0 and d3 <= 0.0:
        t = d1 / (d1 - d3)
        return tuple(a[i] + ab[i] * t for i in range(3))
    cp = sub(point, c)
    d5 = dot(ab, cp)
    d6 = dot(ac, cp)
    if d6 >= 0.0 and d5 <= d6:
        return c
    vb = d5 * d2 - d1 * d6
    if vb <= 0.0 and d2 >= 0.0 and d6 <= 0.0:
        t = d2 / (d2 - d6)
        return tuple(a[i] + ac[i] * t for i in range(3))
    va = d3 * d6 - d5 * d4
    if va <= 0.0 and (d4 - d3) >= 0.0 and (d5 - d6) >= 0.0:
        t = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return tuple(b[i] + (c[i] - b[i]) * t for i in range(3))
    denom = 1.0 / (va + vb + vc)
    v = vb * denom
    w = vc * denom
    return tuple(a[i] + ab[i] * v + ac[i] * w for i in range(3))
