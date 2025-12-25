#!/usr/bin/env python3
"""Generate a rough provinces.json for the campaign map.

This is intentionally approximate (gameplay-focused), using hand-authored
lon/lat polygons that are projected into UV space and triangulated.

FIXES
- Provinces cannot "cross sea" / keep disconnected land chunks:
  After clipping to land, we keep only the portion(s) of the province geometry
  that are on the same land connected-component as the province anchor point
  (label_lonlat or first city).
- Overlaps resolved by explicit `priority` first, then area tiebreaker.
- Gap filling and final assignment restricted to:
    (a) same land component AND
    (b) same "region theatre" mask (Italy / Balkans / Africa / Iberia / Gaul).
  This prevents Illyria grabbing land on the Italian peninsula even if the land mesh
  connects in the north.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple
import math

try:
    import mapbox_earcut as earcut
except ImportError:  # pragma: no cover
    earcut = None

try:
    from shapely.geometry import shape, Polygon, Point
    from shapely.ops import unary_union
except ImportError:  # pragma: no cover
    shape = None
    Polygon = None
    Point = None
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


def densify_ring(
    ring: List[Tuple[float, float]], step: float
) -> List[Tuple[float, float]]:
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


def expand_ring(
    ring: List[Tuple[float, float]], factor: float
) -> List[Tuple[float, float]]:
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


def jitter_ring(
    ring: List[Tuple[float, float]], amplitude: float, frequency: float
) -> List[Tuple[float, float]]:
    if amplitude <= 0.0 or len(ring) < 2:
        return ring
    output: List[Tuple[float, float]] = []
    for idx, (lon, lat) in enumerate(ring):
        n = math.sin(idx * frequency) * 0.5 + math.sin(idx * frequency * 0.37) * 0.5
        output.append((lon + n * amplitude, lat + n * amplitude * 0.7))
    return output


def clamp_ring(
    ring: List[Tuple[float, float]], bounds: MapBounds
) -> List[Tuple[float, float]]:
    output: List[Tuple[float, float]] = []
    for lon, lat in ring:
        clamped_lon = min(max(lon, bounds.lon_min), bounds.lon_max)
        clamped_lat = min(max(lat, bounds.lat_min), bounds.lat_max)
        output.append((clamped_lon, clamped_lat))
    return output


def extract_polygons(geom):
    """Extract all polygons from a geometry."""
    if geom.is_empty:
        return []
    if geom.geom_type == "Polygon":
        return [geom]
    if geom.geom_type in ("MultiPolygon", "GeometryCollection"):
        polys = []
        for g in geom.geoms:
            polys.extend(extract_polygons(g))
        return polys
    return []


def build_region_masks(bounds: MapBounds) -> dict[str, Polygon]:
    """
    Coarse "theatre" masks. Provinces are constrained to their theatre to prevent
    land-bridge bleed (Italy <-> Balkans connection in the north).

    These are intentionally simple rectangles-ish in lon/lat to match your hand-made map.
    Adjust corners if your map bounds differ a lot.
    """
    region_masks_lonlat = {
        # Italian peninsula + Po valley (includes N Italy)
        "italy": [
            (6.0, 47.8),
            (19.2, 47.8),
            (19.2, 36.0),
            (6.0, 36.0),
            (6.0, 47.8),
        ],
        # Balkans / Illyria / Greece-side (east of Adriatic)
        "balkans": [
            (12.8, 47.8),
            (26.0, 47.8),
            (26.0, 36.0),
            (12.8, 36.0),
            (12.8, 47.8),
        ],
        # North Africa theatre
        "africa": [
            (-11.0, 38.8),
            (26.0, 38.8),
            (26.0, 20.0),
            (-11.0, 20.0),
            (-11.0, 38.8),
        ],
        # Iberia theatre
        "iberia": [
            (-11.0, 45.5),
            (5.5, 45.5),
            (5.5, 34.5),
            (-11.0, 34.5),
            (-11.0, 45.5),
        ],
        # Gaul / southern France theatre
        "gaul": [
            (-6.0, 50.5),
            (16.0, 50.5),
            (16.0, 41.0),
            (-6.0, 41.0),
            (-6.0, 50.5),
        ],
    }

    masks: dict[str, Polygon] = {}
    for key, ring_ll in region_masks_lonlat.items():
        ring_uv = [bounds.to_uv(lon, lat) for lon, lat in ring_ll]
        poly = Polygon(ring_uv)
        if not poly.is_valid:
            poly = poly.buffer(0)
        masks[key] = poly
    return masks


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
            "region": "iberia",
            "priority": 500,
            "jitter_amplitude": 0.1,
            "expand_factor": 1.02,
            "cities": [
                {"name": "New Carthage", "lonlat": (-0.98, 37.6)},
                {"name": "Saguntum", "lonlat": (-0.3, 39.7)},
                {"name": "Tarraco", "lonlat": (1.25, 41.1)},
                {"name": "Emporiae", "lonlat": (3.1, 42.1)},
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
            "region": "iberia",
            "priority": 200,
            "jitter_amplitude": 0.1,
            "expand_factor": 1.02,
            "cities": [{"name": "Numantia", "lonlat": (-2.4, 41.8)}],
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
            "region": "gaul",
            "priority": 200,
            "jitter_amplitude": 0.08,
            "expand_factor": 1.01,
            "cities": [{"name": "Massalia", "lonlat": (5.4, 43.3)}],
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
            "region": "italy",
            "priority": 200,
            "jitter_amplitude": 0.08,
            "expand_factor": 1.01,
            "cities": [
                {"name": "Mediolanum", "lonlat": (9.2, 45.5)},
                {"name": "Placentia", "lonlat": (9.7, 45.0)},
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
            "region": "italy",
            "priority": 900,
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [
                {"name": "Veii", "lonlat": (12.3, 42.5)},
                {"name": "Ariminum", "lonlat": (12.6, 44.0)},
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
            "region": "italy",
            "priority": 1000,
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [{"name": "Rome", "lonlat": (12.5, 41.9)}],
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
            "owner": "rome",
            "region": "italy",
            "priority": 850,
            "jitter_amplitude": 0.06,
            "expand_factor": 1.03,
            "cities": [
                {"name": "Capua", "lonlat": (14.3, 41.1)},
                {"name": "Tarentum", "lonlat": (17.2, 40.5)},
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
            "name": "Sicily",
            "owner": "rome",
            "region": "italy",
            "priority": 880,
            "jitter_amplitude": 0.04,
            "expand_factor": 1.02,
            "cities": [
                {"name": "Syracuse", "lonlat": (15.3, 37.1)},
                {"name": "Lilybaeum", "lonlat": (12.8, 38.6)},
            ],
            "lonlat": [
                (12.4, 38.3),
                (13.6, 38.3),
                (15.7, 38.3),
                (15.6, 37.6),
                (15.3, 37.0),
                (15.0, 36.7),
                (14.2, 36.6),
                (13.2, 36.6),
                (12.3, 37.1),
                (12.2, 37.8),
                (12.4, 38.3),
            ],
            "label_lonlat": (14.0, 37.6),
        },
        {
            "id": "sardinia",
            "name": "Sardinia",
            "owner": "rome",
            "region": "italy",
            "priority": 800,
            "jitter_amplitude": 0.04,
            "expand_factor": 1.05,
            "cities": [{"name": "Caralis", "lonlat": (9.1, 39.2)}],
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
            "region": "italy",
            "priority": 800,
            "jitter_amplitude": 0.04,
            "expand_factor": 1.05,
            "cities": [{"name": "Aleria", "lonlat": (9.5, 42.1)}],
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
            "region": "africa",
            "priority": 700,
            "jitter_amplitude": 0.05,
            "expand_factor": 1.03,
            "cities": [{"name": "Carthage", "lonlat": (10.3, 36.85)}],
            "lonlat": [
                (7.6, 37.6),
                (12.4, 37.6),
                (12.7, 36.2),
                (12.4, 33.0),
                (9.6, 32.4),
                (7.4, 33.8),
                (7.6, 37.6),
            ],
            "label_lonlat": (10.6, 36.9),
        },
        {
            "id": "numidia",
            "name": "Numidia",
            "owner": "carthage",
            "region": "africa",
            "priority": 650,
            "jitter_amplitude": 0.06,
            "expand_factor": 1.02,
            "cities": [
                {"name": "Cirta", "lonlat": (6.6, 36.4)},
                {"name": "Tingis", "lonlat": (-5.5, 35.2)},
            ],
            "lonlat": [
                (-8.8, 36.8),
                (7.6, 36.8),
                (7.6, 35.8),
                (6.2, 34.9),
                (2.5, 34.2),
                (-2.0, 34.0),
                (-6.5, 34.6),
                (-8.8, 35.4),
                (-8.8, 36.8),
            ],
            "label_lonlat": (-1.0, 35.6),
        },
        {
            "id": "libya",
            "name": "Libya",
            "owner": "neutral",
            "region": "africa",
            "priority": 100,
            "jitter_amplitude": 0.05,
            "expand_factor": 1.02,
            "cities": [{"name": "Leptis", "lonlat": (14.5, 32.2)}],
            "lonlat": [
                (12.0, 37.2),
                (18.8, 37.0),
                (18.8, 28.0),
                (12.2, 28.0),
                (12.0, 37.2),
            ],
            "label_lonlat": (14.8, 35.0),
        },
        {
            "id": "illyria",
            "name": "Illyria",
            "owner": "neutral",
            "region": "balkans",
            "priority": 120,
            "jitter_amplitude": 0.06,
            "expand_factor": 1.0,
            "cities": [{"name": "Salona", "lonlat": (16.4, 43.5)}],
            "lonlat": [
                (15.1, 45.8),
                (18.0, 45.8),
                (20.4, 44.6),
                (20.4, 41.2),
                (18.8, 40.6),
                (15.9, 41.6),
                (15.1, 43.0),
                (15.1, 45.8),
            ],
            "label_lonlat": (16.9, 43.7),
        },
    ]

    if shape is None or unary_union is None or Polygon is None or Point is None:
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

    region_masks = build_region_masks(bounds)

    land_components = (
        list(land_union.geoms)
        if land_union.geom_type == "MultiPolygon"
        else [land_union]
    )

    def land_component_index(pt: Point) -> int | None:
        for i, comp in enumerate(land_components):
            if pt.within(comp):
                return i
        return None

    def choose_anchor_lonlat(prov: dict) -> Tuple[float, float] | None:
        if prov.get("label_lonlat") is not None:
            return tuple(prov["label_lonlat"])
        cities = prov.get("cities") or []
        if cities and cities[0].get("lonlat"):
            ll = cities[0]["lonlat"]
            return (float(ll[0]), float(ll[1]))
        return None

    def keep_only_anchor_component(clipped_geom, anchor_comp: int | None):
        if clipped_geom.is_empty:
            return clipped_geom
        if anchor_comp is None:
            polys = extract_polygons(clipped_geom)
            if not polys:
                return clipped_geom
            polys.sort(key=lambda p: p.area, reverse=True)
            return polys[0]

        comp = land_components[anchor_comp]
        kept = clipped_geom.intersection(comp)
        if kept.is_empty:
            polys = extract_polygons(clipped_geom)
            if not polys:
                return clipped_geom
            polys.sort(key=lambda p: p.area, reverse=True)
            return polys[0]
        return kept

    def region_of_point(pt: Point) -> str | None:
        for key, mask in region_masks.items():
            if pt.within(mask):
                return key
        return None

    def apply_region_mask(geom, region_key: str | None):
        if geom.is_empty or not region_key:
            return geom
        mask = region_masks.get(region_key)
        if mask is None:
            return geom
        return geom.intersection(mask)

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

        anchor_ll = choose_anchor_lonlat(province)
        anchor_comp = None
        if anchor_ll is not None:
            au, av = bounds.to_uv(anchor_ll[0], anchor_ll[1])
            anchor_comp = land_component_index(Point(float(au), float(av)))

        province_defs.append(
            {
                "id": province["id"],
                "name": province["name"],
                "owner": province.get("owner", "neutral"),
                "region": province.get("region"),
                "priority": int(province.get("priority", 0)),
                "poly": poly,
                "label_lonlat": province.get("label_lonlat"),
                "cities": province.get("cities", []),
                "centroid": poly.representative_point(),
                "anchor_comp": anchor_comp,
            }
        )

    print(f"Prepared province shapes: {len(province_defs)}")

    # Step 1: Clip each province to land + drop cross-sea fragments + theatre mask
    province_clipped = {}
    for prov in province_defs:
        clipped = prov["poly"].intersection(land_union)
        clipped = keep_only_anchor_component(clipped, prov["anchor_comp"])
        clipped = apply_region_mask(clipped, prov.get("region"))

        if not clipped.is_empty:
            province_clipped[prov["id"]] = clipped
        else:
            print(f"Warning: Province {prov['id']} has no land intersection")

    # Step 2: Resolve overlaps by explicit priority (then by area)
    prov_meta = {p["id"]: p for p in province_defs if p["id"] in province_clipped}
    province_priority = {
        pid: (prov_meta[pid]["priority"], prov_meta[pid]["poly"].area)
        for pid in province_clipped.keys()
    }
    sorted_provs = sorted(
        province_priority.items(), key=lambda x: (x[1][0], x[1][1]), reverse=True
    )

    resolved_provinces = {}
    for i, (prov_id, _) in enumerate(sorted_provs):
        current_geom = province_clipped[prov_id]
        for j in range(i):
            higher_prov_id, _ = sorted_provs[j]
            if higher_prov_id in resolved_provinces:
                current_geom = current_geom.difference(
                    resolved_provinces[higher_prov_id]
                )
                if current_geom.is_empty:
                    break
        if not current_geom.is_empty:
            resolved_provinces[prov_id] = current_geom

    # Step 3: Gap fill (restricted to same land component AND same region)
    def get_uncovered_land():
        if not resolved_provinces:
            return land_union
        covered = unary_union(list(resolved_provinces.values()))
        return land_union.difference(covered)

    MAX_ITERATIONS = 50
    MIN_GAP_AREA = 1e-9
    EXPANSION_STEP = 0.002

    for iteration in range(MAX_ITERATIONS):
        uncovered = get_uncovered_land()
        if uncovered.is_empty or uncovered.area < MIN_GAP_AREA:
            print(f"All land covered after {iteration} iterations")
            break

        print(
            f"Gap fill iteration {iteration + 1}/{MAX_ITERATIONS}: uncovered area {uncovered.area:.6f}"
        )

        uncovered_polys = extract_polygons(uncovered)
        if not uncovered_polys:
            break

        made_progress = False

        for gap_poly in uncovered_polys:
            if gap_poly.area < MIN_GAP_AREA:
                continue

            gap_pt = gap_poly.representative_point()
            gap_comp = land_component_index(gap_pt)
            gap_region = region_of_point(gap_pt)

            candidates = []
            for prov_id, geom in resolved_provinces.items():
                if (
                    gap_comp is not None
                    and prov_meta.get(prov_id, {}).get("anchor_comp") != gap_comp
                ):
                    continue
                if (
                    gap_region is not None
                    and prov_meta.get(prov_id, {}).get("region") != gap_region
                ):
                    continue
                dist = gap_poly.distance(geom)
                if dist < EXPANSION_STEP * 2:
                    candidates.append((dist, prov_id))

            if not candidates:
                for prov_id, geom in resolved_provinces.items():
                    if (
                        gap_comp is not None
                        and prov_meta.get(prov_id, {}).get("anchor_comp") != gap_comp
                    ):
                        continue
                    if (
                        gap_region is not None
                        and prov_meta.get(prov_id, {}).get("region") != gap_region
                    ):
                        continue
                    candidates.append((gap_poly.distance(geom), prov_id))

            if not candidates:
                continue

            candidates.sort()
            remaining_gap = gap_poly

            for _, prov_id in candidates[:3]:
                if remaining_gap.is_empty or remaining_gap.area < MIN_GAP_AREA:
                    break

                expanded = resolved_provinces[prov_id].buffer(EXPANSION_STEP)
                claim = expanded.intersection(remaining_gap)
                if claim.is_empty or claim.area < MIN_GAP_AREA:
                    continue

                claim = claim.intersection(land_union)
                claim = keep_only_anchor_component(
                    claim, prov_meta[prov_id]["anchor_comp"]
                )
                claim = apply_region_mask(claim, prov_meta[prov_id].get("region"))

                if not claim.is_empty and claim.area > MIN_GAP_AREA:
                    resolved_provinces[prov_id] = resolved_provinces[prov_id].union(
                        claim
                    )
                    remaining_gap = remaining_gap.difference(claim)
                    made_progress = True

        if not made_progress:
            EXPANSION_STEP *= 1.5
            if EXPANSION_STEP > 0.05:
                print(f"Warning: Could not fill all gaps after {iteration} iterations")
                break

    # Step 4: Final pass - assign remaining uncovered land to nearest province (same comp + same region)
    uncovered = get_uncovered_land()
    if not uncovered.is_empty and uncovered.area > MIN_GAP_AREA:
        print(f"Final pass: {uncovered.area:.6f} uncovered area remaining")
        for gap_poly in extract_polygons(uncovered):
            if gap_poly.area < MIN_GAP_AREA:
                continue

            centroid = gap_poly.representative_point()
            gap_comp = land_component_index(centroid)
            gap_region = region_of_point(centroid)

            best_prov = None
            best_dist = float("inf")

            for prov_id, geom in resolved_provinces.items():
                if (
                    gap_comp is not None
                    and prov_meta.get(prov_id, {}).get("anchor_comp") != gap_comp
                ):
                    continue
                if (
                    gap_region is not None
                    and prov_meta.get(prov_id, {}).get("region") != gap_region
                ):
                    continue
                d = centroid.distance(geom)
                if d < best_dist:
                    best_dist = d
                    best_prov = prov_id

            if best_prov is not None:
                claim = keep_only_anchor_component(
                    gap_poly, prov_meta[best_prov]["anchor_comp"]
                )
                claim = apply_region_mask(claim, prov_meta[best_prov].get("region"))
                resolved_provinces[best_prov] = resolved_provinces[best_prov].union(
                    claim
                )

    # Step 5: Re-resolve overlaps (priority order)
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
            if not current_geom.is_valid:
                current_geom = current_geom.buffer(0)
            final_provinces[prov_id] = current_geom

    resolved_provinces = final_provinces

    # Report coverage
    final_uncovered = get_uncovered_land()
    if final_uncovered.is_empty or final_uncovered.area < MIN_GAP_AREA:
        print("100% land coverage achieved")
    else:
        total_land = land_union.area
        coverage = (total_land - final_uncovered.area) / total_land * 100
        print(f"Land coverage: {coverage:.2f}% ({final_uncovered.area:.6f} uncovered)")

    # Step 6: Triangulate
    province_triangles = {}
    for prov_id, geom in resolved_provinces.items():
        tris = []
        for poly in extract_polygons(geom):
            tris.extend(triangulate_polygon(poly))
        province_triangles[prov_id] = tris

    # Step 7: Output
    output: List[dict] = []
    borders: List[List[List[float]]] = []

    for prov in province_defs:
        prov_id = prov["id"]
        tris = province_triangles.get(prov_id, [])
        if not tris:
            print(f"Warning: Province {prov_id} has no triangles after processing")
            continue

        if prov_id in resolved_provinces:
            for poly in extract_polygons(resolved_provinces[prov_id]):
                border = [[float(u), float(v)] for (u, v) in poly.exterior.coords]
                if len(border) >= 2:
                    borders.append(border)

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
                "label_uv": [float(label_uv[0]), float(label_uv[1])],
                "triangles": [[float(u), float(v)] for (u, v) in tris],
                "cities": cities,
            }
        )

    return output, borders


def main() -> None:
    bounds = MapBounds.from_json(BOUNDS_PATH)
    provinces, borders = build_provinces(bounds)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(
        json.dumps({"provinces": provinces, "borders": borders}, indent=2)
    )
    print(f"Wrote {len(provinces)} provinces to {OUT_PATH}")


if __name__ == "__main__":
    main()
