#!/usr/bin/env python3
"""Generate a rough provinces.json for the campaign map.

This is intentionally approximate (gameplay-focused), using hand-authored
lon/lat polygons that are projected into UV space and triangulated.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple
import math

try:
    import mapbox_earcut as earcut
except ImportError:  # pragma: no cover - optional dependency
    earcut = None

try:
    from shapely.geometry import shape, Polygon
    from shapely.ops import unary_union
except ImportError:  # pragma: no cover - optional dependency
    shape = None
    Polygon = None
    unary_union = None

ROOT = Path(__file__).resolve().parents[2]
BOUNDS_PATH = ROOT / "tools" / "map_pipeline" / "map_bounds.json"
OUT_PATH = ROOT / "assets" / "campaign_map" / "provinces.json"
LAND_UV_PATH = ROOT / "assets" / "campaign_map" / "land_uv.geojson"


@dataclass(frozen=True)
class MapBounds:
    lon_min: float
    lon_max: float
    lat_min: float
    lat_max: float

    @staticmethod
    def from_json(path: Path) -> "MapBounds":
        data = json.loads(path.read_text())
        return MapBounds(
            lon_min=float(data["lon_min"]),
            lon_max=float(data["lon_max"]),
            lat_min=float(data["lat_min"]),
            lat_max=float(data["lat_max"]),
        )

    def to_uv(self, lon: float, lat: float) -> Tuple[float, float]:
        u = (lon - self.lon_min) / (self.lon_max - self.lon_min)
        v = (lat - self.lat_min) / (self.lat_max - self.lat_min)
        return (u, v)


def triangulate_polygon(poly) -> List[List[float]]:
    if earcut is None:
        raise RuntimeError("mapbox_earcut is required to triangulate provinces.")

    rings: List[List[Tuple[float, float]]] = []
    rings.append(list(poly.exterior.coords)[:-1])
    for interior in poly.interiors:
        rings.append(list(interior.coords)[:-1])

    verts: List[Tuple[float, float]] = []
    ring_end_indices: List[int] = []
    for ring in rings:
        verts.extend((float(u), float(v)) for (u, v) in ring)
        ring_end_indices.append(len(verts))

    if not verts:
        return []

    import numpy as np

    v_arr = np.array(verts, dtype=np.float32).reshape((-1, 2))
    end_arr = np.array(ring_end_indices, dtype=np.uint32)
    try:
        indices = earcut.triangulate_float32(v_arr, end_arr)
    except Exception:
        return []

    triangles: List[List[float]] = []
    for idx in indices:
        u, v = verts[int(idx)]
        triangles.append([float(u), float(v)])
    return triangles


def triangulate_land(land_geom) -> List[List[List[float]]]:
    triangles: List[List[List[float]]] = []
    polys = [land_geom] if land_geom.geom_type == "Polygon" else list(land_geom.geoms)
    for poly in polys:
        tri = triangulate_polygon(poly)
        for i in range(0, len(tri), 3):
            if i + 2 < len(tri):
                triangles.append([tri[i], tri[i + 1], tri[i + 2]])
    return triangles


def densify_ring(ring: List[Tuple[float, float]], step: float) -> List[Tuple[float, float]]:
    if len(ring) < 2:
        return ring
    output: List[Tuple[float, float]] = []
    for a, b in zip(ring, ring[1:]):
        output.append(a)
        dx = b[0] - a[0]
        dy = b[1] - a[1]
        dist = max(abs(dx), abs(dy))
        steps = max(1, int(dist / max(step, 1e-6)))
        for i in range(1, steps):
            t = i / steps
            output.append((a[0] + dx * t, a[1] + dy * t))
    output.append(ring[-1])
    return output


def expand_ring(ring: List[Tuple[float, float]], factor: float) -> List[Tuple[float, float]]:
    if len(ring) < 3:
        return ring
    center_lon = sum(lon for lon, _ in ring) / len(ring)
    center_lat = sum(lat for _, lat in ring) / len(ring)
    output: List[Tuple[float, float]] = []
    for lon, lat in ring:
        output.append(
            (
                center_lon + (lon - center_lon) * factor,
                center_lat + (lat - center_lat) * factor,
            )
        )
    return output


def jitter_ring(ring: List[Tuple[float, float]], amplitude: float, frequency: float) -> List[Tuple[float, float]]:
    if amplitude <= 0.0 or len(ring) < 2:
        return ring
    output: List[Tuple[float, float]] = []
    for idx, (lon, lat) in enumerate(ring):
        n = math.sin(idx * frequency) * 0.5 + math.sin(idx * frequency * 0.37) * 0.5
        output.append((lon + n * amplitude, lat + n * amplitude * 0.7))
    return output


def clamp_ring(ring: List[Tuple[float, float]], bounds: MapBounds) -> List[Tuple[float, float]]:
    output: List[Tuple[float, float]] = []
    for lon, lat in ring:
        clamped_lon = min(max(lon, bounds.lon_min), bounds.lon_max)
        clamped_lat = min(max(lat, bounds.lat_min), bounds.lat_max)
        output.append((clamped_lon, clamped_lat))
    return output


def build_provinces(bounds: MapBounds) -> tuple[List[dict], List[List[List[float]]]]:
    owner_palette = {
        "rome": [0.82, 0.12, 0.08, 0.42],
        "carthage": [0.8, 0.56, 0.28, 0.38],
        "neutral": [0.2, 0.2, 0.2, 0.32],
    }

    provinces = [
        {
            "id": "iberia_carthaginian",
            "name": "Carthaginian Iberia",
            "owner": "carthage",
            "jitter_amplitude": 0.1,
            "expand_factor": 1.02,
            "cities": [
                {"name": "New Carthage", "lonlat": (-0.98, 37.6)},
            ],
            "lonlat": [
                (-6.5, 40.5),
                (2.5, 40.8),
                (3.5, 39.0),
                (2.0, 37.0),
                (-1.0, 36.2),
                (-5.5, 36.3),
                (-7.0, 38.0),
                (-6.5, 40.5),
            ],
            "label_lonlat": (-1.5, 38.5),
        },
        {
            "id": "iberia_interior",
            "name": "Iberian Interior",
            "owner": "neutral",
            "jitter_amplitude": 0.1,
            "expand_factor": 1.02,
            "cities": [
                {"name": "Numantia", "lonlat": (-2.4, 41.8)},
            ],
            "lonlat": [
                (-9.5, 43.5),
                (-1.0, 43.8),
                (2.5, 41.0),
                (-6.5, 40.5),
                (-7.0, 38.0),
                (-9.0, 36.0),
                (-9.5, 43.5),
            ],
            "label_lonlat": (-5.0, 41.5),
        },
        {
            "id": "transalpine_gaul",
            "name": "Transalpine Gaul",
            "owner": "neutral",
            "jitter_amplitude": 0.08,
            "expand_factor": 1.01,
            "cities": [
                {"name": "Massalia", "lonlat": (5.4, 43.3)},
            ],
            "lonlat": [
                (-1.5, 45.6),
                (6.5, 45.6),
                (7.3, 44.2),
                (6.8, 43.0),
                (3.0, 42.8),
                (0.0, 43.2),
                (-1.5, 44.2),
                (-1.5, 45.6),
            ],
            "label_lonlat": (2.5, 44.3),
        },
        {
            "id": "cisalpine_gaul",
            "name": "Cisalpine Gaul",
            "owner": "neutral",
            "jitter_amplitude": 0.08,
            "expand_factor": 1.01,
            "cities": [
                {"name": "Mediolanum", "lonlat": (9.2, 45.5)},
            ],
            "lonlat": [
                (6.0, 46.6),
                (12.5, 46.6),
                (13.2, 45.3),
                (10.5, 44.4),
                (7.0, 44.6),
                (6.0, 46.6),
            ],
            "label_lonlat": (9.5, 45.5),
        },
        {
            "id": "etruria",
            "name": "Etruria",
            "owner": "rome",
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [
                {"name": "Veii", "lonlat": (12.3, 42.0)},
            ],
            "lonlat": [
                (8.6, 44.4),
                (11.9, 44.4),
                (12.4, 43.1),
                (10.2, 41.8),
                (8.8, 42.7),
                (8.6, 44.4),
            ],
            "label_lonlat": (10.5, 43.3),
        },
        {
            "id": "roman_core",
            "name": "Roman Core",
            "owner": "rome",
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [
                {"name": "Rome", "lonlat": (12.5, 41.9)},
            ],
            "lonlat": [
                (9.9, 42.7),
                (13.3, 42.6),
                (13.8, 41.0),
                (12.2, 40.2),
                (10.2, 40.6),
                (9.9, 42.7),
            ],
            "label_lonlat": (12.0, 41.6),
        },
        {
            "id": "southern_italy",
            "name": "Southern Italy",
            "owner": "neutral",
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [
                {"name": "Capua", "lonlat": (14.3, 41.1)},
            ],
            "lonlat": [
                (11.6, 41.2),
                (15.9, 41.2),
                (17.2, 39.2),
                (16.2, 37.6),
                (12.8, 37.4),
                (11.6, 38.7),
                (11.6, 41.2),
            ],
            "label_lonlat": (14.3, 39.6),
        },
        {
            "id": "sicily_roman",
            "name": "Sicily (Roman)",
            "owner": "rome",
            "jitter_amplitude": 0.04,
            "expand_factor": 1.0,
            "cities": [
                {"name": "Syracuse", "lonlat": (15.3, 37.1)},
            ],
            "lonlat": [
                (12.8, 38.3),
                (15.7, 38.3),
                (15.4, 37.0),
                (13.4, 36.9),
                (12.8, 38.3),
            ],
            "label_lonlat": (14.2, 37.7),
        },
        {
            "id": "sicily_carthaginian",
            "name": "Sicily (Carthage)",
            "owner": "carthage",
            "jitter_amplitude": 0.03,
            "expand_factor": 1.0,
            "cities": [
                {"name": "Lilybaeum", "lonlat": (12.5, 37.8)},
            ],
            "lonlat": [
                (12.2, 38.3),
                (13.1, 38.3),
                (13.0, 37.1),
                (12.3, 37.2),
                (12.2, 38.3),
            ],
            "label_lonlat": (12.6, 37.7),
        },
        {
            "id": "sardinia",
            "name": "Sardinia",
            "owner": "rome",
            "jitter_amplitude": 0.04,
            "expand_factor": 1.0,
            "cities": [
                {"name": "Caralis", "lonlat": (9.1, 39.2)},
            ],
            "lonlat": [
                (8.0, 41.3),
                (9.5, 41.3),
                (9.6, 38.9),
                (8.1, 38.7),
                (8.0, 41.3),
            ],
            "label_lonlat": (8.8, 40.1),
        },
        {
            "id": "corsica",
            "name": "Corsica",
            "owner": "rome",
            "jitter_amplitude": 0.04,
            "expand_factor": 1.0,
            "cities": [
                {"name": "Aleria", "lonlat": (9.5, 42.1)},
            ],
            "lonlat": [
                (8.6, 43.0),
                (9.7, 43.0),
                (9.6, 41.4),
                (8.7, 41.4),
                (8.6, 43.0),
            ],
            "label_lonlat": (9.1, 42.2),
        },
        {
            "id": "carthage_core",
            "name": "Carthage Core",
            "owner": "carthage",
            "jitter_amplitude": 0.05,
            "expand_factor": 1.06,
            "cities": [
                {"name": "Carthage", "lonlat": (10.3, 36.85)},
            ],
            "lonlat": [
                (7.8, 38.3),
                (13.8, 38.3),
                (14.2, 36.2),
                (12.8, 35.2),
                (9.0, 35.2),
                (7.8, 36.8),
                (7.8, 38.3),
            ],
            "label_lonlat": (10.6, 36.9),
        },
        {
            "id": "numidia",
            "name": "Numidia",
            "owner": "carthage",
            "jitter_amplitude": 0.06,
            "expand_factor": 1.02,
            "cities": [
                {"name": "Cirta", "lonlat": (6.6, 36.4)},
            ],
            "lonlat": [
                (-10.0, 37.4),
                (7.6, 37.4),
                (7.6, 38.3),
                (1.5, 38.0),
                (-4.0, 37.6),
                (-8.5, 37.2),
                (-10.0, 37.4),
                (-9.0, 35.6),
                (-6.0, 34.6),
                (-2.0, 34.0),
                (2.5, 34.2),
                (6.5, 34.8),
                (7.6, 35.8),
                (-10.0, 35.8),
                (-10.0, 37.4),
            ],
            "label_lonlat": (-1.0, 35.6),
        },
        {
            "id": "libya",
            "name": "Libya",
            "owner": "neutral",
            "jitter_amplitude": 0.05,
            "expand_factor": 1.02,
            "cities": [
                {"name": "Leptis", "lonlat": (14.5, 32.6)},
            ],
            "lonlat": [
                (12.2, 37.2),
                (18.0, 37.0),
                (18.0, 31.0),
                (12.6, 31.0),
                (12.2, 37.2),
            ],
            "label_lonlat": (14.8, 35.0),
        },
        {
            "id": "illyria",
            "name": "Illyria",
            "owner": "neutral",
            "jitter_amplitude": 0.06,
            "expand_factor": 1.0,
            "cities": [
                {"name": "Salona", "lonlat": (16.4, 43.5)},
            ],
            "lonlat": [
                (12.5, 45.5),
                (16.5, 45.5),
                (18.0, 44.5),
                (18.0, 42.2),
                (15.0, 41.3),
                (13.0, 42.2),
                (12.5, 45.5),
            ],
            "label_lonlat": (15.4, 43.7),
        },
    ]

    if shape is None or unary_union is None or Polygon is None:
        raise RuntimeError("Shapely is required to clip provinces to land.")
    if earcut is None:
        raise RuntimeError("mapbox_earcut is required to triangulate provinces.")
    if not LAND_UV_PATH.exists():
        raise RuntimeError(f"Missing land UV data at {LAND_UV_PATH}")

    land_geo = json.loads(LAND_UV_PATH.read_text())
    geoms = [shape(feat["geometry"]) for feat in land_geo.get("features", [])]
    if not geoms:
        raise RuntimeError("No land geometry found in land_uv.geojson")
    land_union = unary_union(geoms)
    print(f"Loaded land geometry: {len(geoms)} feature(s)")

    province_defs = []
    for province in provinces:
        ring = province["lonlat"]
        expand_factor = province.get("expand_factor", 1.0)
        if expand_factor != 1.0:
            ring = expand_ring(ring, factor=expand_factor)

        densify_step = province.get("densify_step", 0.6)
        ring = densify_ring(ring, step=densify_step)

        jitter_amp = province.get("jitter_amplitude", 0.1)
        jitter_freq = province.get("jitter_frequency", 1.5)
        if jitter_amp > 0.0:
            ring = jitter_ring(ring, amplitude=jitter_amp, frequency=jitter_freq)

        ring = clamp_ring(ring, bounds)
        uv_ring = [bounds.to_uv(lon, lat) for lon, lat in ring]

        poly = Polygon(uv_ring)
        if not poly.is_valid:
            poly = poly.buffer(0)
        if poly.is_empty:
            continue

        province_defs.append(
            {
                "id": province["id"],
                "name": province["name"],
                "owner": province.get("owner", "neutral"),
                "poly": poly,
                "label_lonlat": province.get("label_lonlat"),
                "cities": province.get("cities", []),
                "centroid": poly.representative_point(),
            }
        )
    print(f"Prepared province shapes: {len(province_defs)}")

    # Step 1: Clip each province to land (preserving original shapes)
    province_clipped = {}
    for prov in province_defs:
        clipped = prov["poly"].intersection(land_union)
        if not clipped.is_empty:
            province_clipped[prov["id"]] = clipped
        else:
            print(f"Warning: Province {prov['id']} has no land intersection")

    # Step 2: Resolve overlaps locally (only where provinces overlap)
    # Use original polygon area for priority (larger original = higher priority)
    province_priority = {
        prov["id"]: prov["poly"].area
        for prov in province_defs
        if prov["id"] in province_clipped
    }
    sorted_provs = sorted(province_priority.items(), key=lambda x: x[1], reverse=True)

    # Subtract higher-priority provinces from lower-priority ones
    resolved_provinces = {}
    for i, (prov_id, _) in enumerate(sorted_provs):
        current_geom = province_clipped[prov_id]

        # Subtract all higher-priority provinces
        for j in range(i):
            higher_prov_id, _ = sorted_provs[j]
            if higher_prov_id in resolved_provinces:
                current_geom = current_geom.difference(resolved_provinces[higher_prov_id])
                if current_geom.is_empty:
                    break

        if not current_geom.is_empty:
            resolved_provinces[prov_id] = current_geom

    # Step 3: Fill all uncovered land (gaps + shoreline) iteratively
    def get_uncovered_land():
        if not resolved_provinces:
            return land_union
        covered = unary_union(list(resolved_provinces.values()))
        return land_union.difference(covered)

    def extract_polygons(geom):
        """Extract all polygons from a geometry."""
        if geom.is_empty:
            return []
        if geom.geom_type == "Polygon":
            return [geom]
        elif geom.geom_type in ("MultiPolygon", "GeometryCollection"):
            polys = []
            for g in geom.geoms:
                polys.extend(extract_polygons(g))
            return polys
        return []

    # Iteratively expand provinces to fill gaps
    MAX_ITERATIONS = 50
    MIN_GAP_AREA = 1e-9  # Tiny threshold to consider "filled"
    EXPANSION_STEP = 0.002  # Buffer step size in UV space

    for iteration in range(MAX_ITERATIONS):
        uncovered = get_uncovered_land()
        if uncovered.is_empty or uncovered.area < MIN_GAP_AREA:
            print(f"All land covered after {iteration} iterations")
            break
        print(f"Gap fill iteration {iteration + 1}/{MAX_ITERATIONS}: uncovered area {uncovered.area:.6f}")

        uncovered_polys = extract_polygons(uncovered)
        if not uncovered_polys:
            break

        made_progress = False
        processed_gaps = 0
        for gap_poly in uncovered_polys:
            if gap_poly.area < MIN_GAP_AREA:
                continue

            # Find provinces that touch or are near this gap
            candidates = []
            for prov_id, geom in resolved_provinces.items():
                dist = gap_poly.distance(geom)
                if dist < EXPANSION_STEP * 2:  # Only consider nearby provinces
                    candidates.append((dist, prov_id))

            if not candidates:
                # No nearby province - find the closest one
                for prov_id, geom in resolved_provinces.items():
                    dist = gap_poly.distance(geom)
                    candidates.append((dist, prov_id))

            candidates.sort()

            # Expand each nearby province into the gap proportionally
            remaining_gap = gap_poly
            for dist, prov_id in candidates[:3]:  # Top 3 closest
                if remaining_gap.is_empty or remaining_gap.area < MIN_GAP_AREA:
                    break

                # Expand province towards the gap
                expanded = resolved_provinces[prov_id].buffer(EXPANSION_STEP)
                claim = expanded.intersection(remaining_gap)

                if not claim.is_empty and claim.area > MIN_GAP_AREA:
                    # Clip the claim to land (safety)
                    claim = claim.intersection(land_union)
                    if not claim.is_empty:
                        resolved_provinces[prov_id] = resolved_provinces[prov_id].union(claim)
                        remaining_gap = remaining_gap.difference(claim)
                        made_progress = True
            processed_gaps += 1

        print(f"  processed gaps: {processed_gaps}, progress: {'yes' if made_progress else 'no'}")

        if not made_progress:
            # Force-expand all provinces slightly to catch remaining gaps
            EXPANSION_STEP *= 1.5
            if EXPANSION_STEP > 0.05:
                print(f"Warning: Could not fill all gaps after {iteration} iterations")
                break

    # Step 4: Final pass - assign any remaining uncovered land to nearest province
    uncovered = get_uncovered_land()
    if not uncovered.is_empty and uncovered.area > MIN_GAP_AREA:
        print(f"Final pass: {uncovered.area:.6f} uncovered area remaining")
        uncovered_polys = extract_polygons(uncovered)

        for gap_poly in uncovered_polys:
            if gap_poly.area < MIN_GAP_AREA:
                continue

            # Find the single nearest province and assign the entire gap to it
            centroid = gap_poly.representative_point()
            best_prov = None
            best_dist = float("inf")

            for prov_id, geom in resolved_provinces.items():
                dist = centroid.distance(geom)
                if dist < best_dist:
                    best_dist = dist
                    best_prov = prov_id

            if best_prov:
                resolved_provinces[best_prov] = resolved_provinces[best_prov].union(gap_poly)

    # Step 5: Re-resolve any overlaps created during expansion
    # This ensures no provinces overlap after gap filling
    final_provinces = {}
    for i, (prov_id, _) in enumerate(sorted_provs):
        if prov_id not in resolved_provinces:
            continue
        current_geom = resolved_provinces[prov_id]

        for j in range(i):
            higher_prov_id, _ = sorted_provs[j]
            if higher_prov_id in final_provinces:
                current_geom = current_geom.difference(final_provinces[higher_prov_id])
                if current_geom.is_empty:
                    break

        if not current_geom.is_empty:
            # Clean up geometry
            if not current_geom.is_valid:
                current_geom = current_geom.buffer(0)
            final_provinces[prov_id] = current_geom

    resolved_provinces = final_provinces

    # Report final coverage
    final_uncovered = get_uncovered_land()
    if final_uncovered.is_empty or final_uncovered.area < MIN_GAP_AREA:
        print("100% land coverage achieved")
    else:
        total_land = land_union.area
        coverage = (total_land - final_uncovered.area) / total_land * 100
        print(f"Land coverage: {coverage:.2f}% ({final_uncovered.area:.6f} uncovered)")

    # Step 6: Triangulate each province's final geometry
    province_triangles = {}
    for prov_id, geom in resolved_provinces.items():
        tris = []
        polys = extract_polygons(geom)
        for poly in polys:
            tri = triangulate_polygon(poly)
            tris.extend(tri)
        province_triangles[prov_id] = tris

    # Step 7: Generate output with borders from resolved geometry
    output: List[dict] = []
    borders: List[List[List[float]]] = []

    for prov in province_defs:
        prov_id = prov["id"]
        tris = province_triangles.get(prov_id, [])
        if not tris:
            print(f"Warning: Province {prov_id} has no triangles after processing")
            continue

        # Extract borders from resolved geometry (not from triangles)
        if prov_id in resolved_provinces:
            geom = resolved_provinces[prov_id]
            polys = extract_polygons(geom)
            for poly in polys:
                border = [[float(u), float(v)] for (u, v) in poly.exterior.coords]
                if len(border) >= 2:
                    borders.append(border)

        # Use provided label or compute from resolved geometry
        label_lonlat = prov["label_lonlat"]
        if label_lonlat is not None:
            label_uv = bounds.to_uv(label_lonlat[0], label_lonlat[1])
        else:
            if prov_id in resolved_provinces:
                label_pt = resolved_provinces[prov_id].representative_point()
                label_uv = (float(label_pt.x), float(label_pt.y))
            else:
                label_uv = (0.0, 0.0)

        color = owner_palette.get(prov["owner"], owner_palette["neutral"])
        cities = []
        for city in prov.get("cities", []):
            lonlat = city.get("lonlat")
            name = city.get("name", "")
            if not lonlat or len(lonlat) < 2 or not name:
                continue
            uv = bounds.to_uv(lonlat[0], lonlat[1])
            cities.append({"name": name, "uv": [uv[0], uv[1]]})
        output.append(
            {
                "id": prov_id,
                "name": prov["name"],
                "owner": prov["owner"],
                "color": color,
                "label_uv": [label_uv[0], label_uv[1]],
                "triangles": [[float(u), float(v)] for (u, v) in tris],
                "cities": cities,
            }
        )

    return output, borders


def main() -> None:
    bounds = MapBounds.from_json(BOUNDS_PATH)
    provinces, borders = build_provinces(bounds)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"provinces": provinces, "borders": borders}, indent=2))
    print(f"Wrote {len(provinces)} provinces to {OUT_PATH}")


if __name__ == "__main__":
    main()
