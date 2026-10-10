#!/usr/bin/env python3
"""Generate hannibal_path.json with coastline-aware routing and validation.

This script generates 8 progressive paths for Hannibal's campaign, one for each
mission in the Second Punic War campaign. Each path builds upon the previous,
with the final path being the longest and covering all previous waypoints.

Key features:
- Routes follow coastlines with curved paths instead of rigid straight lines
- Each segment is classified as 'coastal', 'land', or 'open_sea'
- Open-sea crossings only occur on explicit segments:
  - Africa → Spain (Tingis → Carteia)
  - Messina Strait (Rhegium → Messana) [short crossing]
  - Sicily → Carthage (Lilybaeum → Carthage)
- Coastal/land segments include intermediate waypoints for smooth appearance
- Extra "steering" waypoints added in problem areas to avoid cutting across water:
  - North Africa: insert Oran between Caesarea and Rusaddir
  - Italy→Sicily: route via Calabria + Messina Strait
  - Sicily traversal: route Syracuse → Gela → Agrigentum → Lilybaeum

Validation system:
- Validates path continuity (no gaps or discontinuities)
- Ensures sea crossings only occur in authorized regions
- Simulates C++ GL_LINE_STRIP rendering to verify visual continuity
- Script exits with error if validation fails

Usage:
    python3 tools/map_pipeline/hannibal_path.py

Requires:
    - map_bounds.json with map boundaries

Outputs:
    - assets/campaign_map/hannibal_path.json (with enhanced coastline-aware paths)

Exit codes:
    0: Success - all paths validated
    1: Failure - validation errors detected
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import List, Tuple

ROOT = Path(__file__).resolve().parents[2]
BOUNDS_PATH = ROOT / "tools" / "map_pipeline" / "map_bounds.json"
OUT_PATH = ROOT / "assets" / "campaign_map" / "hannibal_path.json"


MAX_SEGMENT_DISTANCE = 0.25
SEA_CROSSING_THRESHOLD = 0.10
ALLOWED_SEA_CROSSINGS = {
    "Gibraltar": (0.10, 0.40),
    "Sicily-Channel": (0.72, 0.96),
}


def lon_lat_to_uv(lon: float, lat: float, bounds: dict) -> Tuple[float, float]:
    """Convert lon/lat to UV coordinates based on map bounds."""
    lon_min = bounds["lon_min"]
    lon_max = bounds["lon_max"]
    lat_min = bounds["lat_min"]
    lat_max = bounds["lat_max"]

    u = (lon - lon_min) / (lon_max - lon_min)
    v = (lat - lat_min) / (lat_max - lat_min)
    return (u, v)


def add_coastal_waypoints(
    start: Tuple[float, float],
    end: Tuple[float, float],
    segment_type: str,
    curve_direction: int = 1,
) -> List[Tuple[float, float]]:
    """Add intermediate waypoints for coastal/land segments to create curved paths.

    For open sea segments, uses a straight line.
    curve_direction: 1 for standard perpendicular, -1 for opposite direction
    """
    if segment_type == "open_sea":
        return [start, end]

    su, sv = start
    eu, ev = end

    mid_u = (su + eu) / 2.0
    mid_v = (sv + ev) / 2.0

    du = eu - su
    dv = ev - sv
    perp_u = -dv * curve_direction
    perp_v = du * curve_direction

    length = (perp_u**2 + perp_v**2) ** 0.5
    if length > 0.001:
        perp_u /= length
        perp_v /= length

        if segment_type == "coastal":
            offset = min(0.004, length * 0.03)
            curve_u = mid_u + perp_u * offset * 0.2
            curve_v = mid_v + perp_v * offset * 0.2
        else:
            offset = min(0.010, length * 0.08)
            curve_u = mid_u + perp_u * offset * 0.3
            curve_v = mid_v + perp_v * offset * 0.3

        quarter_u = (su + curve_u) / 2.0
        quarter_v = (sv + curve_v) / 2.0
        three_quarter_u = (curve_u + eu) / 2.0
        three_quarter_v = (curve_v + ev) / 2.0

        return [
            start,
            (quarter_u, quarter_v),
            (curve_u, curve_v),
            (three_quarter_u, three_quarter_v),
            end,
        ]

    return [start, end]


def build_hannibal_path(bounds: dict) -> List[List[List[float]]]:
    """Build 8 progressive paths for Hannibal's campaign with coastline awareness."""

    city_coords = {
        "Carthage": (10.3, 36.65),
        "Hippo_Regius": (7.8, 36.7),
        "Cirta": (6.6, 36.4),
        "Saldae": (5.1, 36.55),
        "Icosium": (3.0, 36.6),
        "Caesarea": (2.2, 36.5),
        "Oran": (-0.62, 35.50),
        "tmp_001": (-3.0, 34.8),
        "Rusaddir": (-5.2, 34.2),
        "Tingis": (-5.8, 35.8),
        "Carteia": (-5.4, 36.4),
        "Malaca": (-4.4, 36.9),
        "Abdera": (-3.0, 36.95),
        "New Carthage": (-1.3, 37.8),
        "Saguntum": (-0.8, 39.7),
        "Tarraco": (0.8, 41.1),
        "Emporiae": (2.8, 42.2),
        "Narbo": (3.0, 43.8),
        "Massalia": (5.1, 43.5),
        "Mediolanum": (9.0, 45.7),
        "Placentia": (9.5, 45.1),
        "Ariminum": (12.4, 44.2),
        "Veii": (12.3, 42.0),
        "Rome": (12.5, 41.9),
        "Capua": (14.3, 41.1),
        "Tarentum": (16.8, 40.6),
        "Croton": (16.3, 39.2),
        "Rhegium": (15.65, 38.11),
        "Messana": (15.55, 38.19),
        "Catana": (15.09, 37.50),
        "Syracuse": (15.1, 37.3),
        "Gela": (14.25, 37.07),
        "Agrigentum": (13.58, 37.31),
        "Lilybaeum": (12.7, 37.8),
    }

    segment_types = {
        ("Carthage", "Hippo_Regius"): ("land", 1),
        ("Hippo_Regius", "Cirta"): ("land", 1),
        ("Cirta", "Saldae"): ("land", 1),
        ("Saldae", "Icosium"): ("land", 1),
        ("Icosium", "Caesarea"): ("land", 1),
        ("Caesarea", "Oran"): ("land", 1),
        ("Oran", "Rusaddir"): ("land", 1),
        ("Rusaddir", "Tingis"): ("coastal", 1),
        ("Tingis", "Carteia"): ("open_sea", 1),
        ("Carteia", "Malaca"): ("land", 1),
        ("Malaca", "Abdera"): ("land", 1),
        ("Abdera", "New Carthage"): ("land", 1),
        ("New Carthage", "Saguntum"): ("land", 1),
        ("Saguntum", "Tarraco"): ("land", 1),
        ("Tarraco", "Emporiae"): ("coastal", 1),
        ("Emporiae", "Narbo"): ("coastal", 1),
        ("Narbo", "Massalia"): ("coastal", 1),
        ("Massalia", "Mediolanum"): ("land", 1),
        ("Mediolanum", "Placentia"): ("land", 1),
        ("Placentia", "Ariminum"): ("land", -1),
        ("Ariminum", "Veii"): ("land", 1),
        ("Veii", "Rome"): ("land", 1),
        ("Rome", "Capua"): ("land", 1),
        ("Capua", "Tarentum"): ("land", -1),
        ("Tarentum", "Croton"): ("coastal", -1),
        ("Croton", "Rhegium"): ("coastal", -1),
        ("Rhegium", "Messana"): ("open_sea", 1),
        ("Messana", "Catana"): ("coastal", 1),
        ("Catana", "Syracuse"): ("coastal", 1),
        ("Syracuse", "Gela"): ("coastal", 1),
        ("Gela", "Agrigentum"): ("coastal", 1),
        ("Agrigentum", "Lilybaeum"): ("coastal", 1),
        ("Lilybaeum", "Carthage"): ("open_sea", 1),
    }

    city_uv: dict[str, Tuple[float, float]] = {}
    for name, (lon, lat) in city_coords.items():
        u, v = lon_lat_to_uv(lon, lat, bounds)
        city_uv[name] = (float(u), float(v))

    BASE_AFRICA_TO_MASSALIA = [
        "Carthage",
        "Hippo_Regius",
        "Cirta",
        "Saldae",
        "Icosium",
        "Caesarea",
        "Oran",
        "Rusaddir",
        "Tingis",
        "Carteia",
        "Malaca",
        "Abdera",
        "New Carthage",
        "Saguntum",
        "Tarraco",
        "Emporiae",
        "Narbo",
        "Massalia",
    ]

    mission_routes = [
        BASE_AFRICA_TO_MASSALIA,
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum"],
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia"],
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia", "Ariminum", "Veii"],
        BASE_AFRICA_TO_MASSALIA
        + ["Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua"],
        BASE_AFRICA_TO_MASSALIA
        + ["Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum"],
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum"],
        BASE_AFRICA_TO_MASSALIA
        + [
            "Mediolanum",
            "Placentia",
            "Ariminum",
            "Veii",
            "Rome",
            "Capua",
            "Tarentum",
            "Croton",
            "Rhegium",
            "Messana",
            "Catana",
            "Syracuse",
            "Gela",
            "Agrigentum",
            "Lilybaeum",
            "Carthage",
        ],
    ]

    lines: List[List[List[float]]] = []
    for idx, route_names in enumerate(mission_routes):
        path_points: List[Tuple[float, float]] = []

        city_waypoints: List[Tuple[float, float]] = []
        for name in route_names:
            uv = city_uv.get(name)
            if uv is None:
                print(
                    f"Warning: Mission {idx} - Hannibal path city '{name}' not found."
                )
                continue
            city_waypoints.append(uv)

        if len(city_waypoints) < 2:
            print(
                f"Warning: Mission {idx} has insufficient waypoints ({len(city_waypoints)}), skipping."
            )
            continue

        for i in range(len(city_waypoints) - 1):
            start = city_waypoints[i]
            end = city_waypoints[i + 1]

            start_city = route_names[i]
            end_city = route_names[i + 1]
            segment_info = segment_types.get((start_city, end_city), ("land", 1))

            if isinstance(segment_info, tuple):
                segment_type, curve_direction = segment_info
            else:
                segment_type = segment_info
                curve_direction = 1

            segment_points = add_coastal_waypoints(
                start, end, segment_type, curve_direction
            )
            path_points.extend(segment_points[:-1])

        path_points.append(city_waypoints[-1])

        line: List[List[float]] = [[float(u), float(v)] for (u, v) in path_points]
        if len(line) >= 2:
            lines.append(line)
        else:
            print(
                f"Warning: Mission {idx} final path has insufficient points ({len(line)}), skipping."
            )

    if not lines:
        print("Warning: No valid paths generated for Hannibal's campaign.")
        return []

    return lines


def validate_path_continuity(path: List[List[float]], mission_idx: int) -> bool:
    """Validate that a path is continuous without gaps."""
    if len(path) < 2:
        print(f"ERROR: Mission {mission_idx} has too few waypoints ({len(path)})")
        return False

    for i in range(len(path) - 1):
        p1 = path[i]
        p2 = path[i + 1]
        distance = ((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2) ** 0.5

        if distance > MAX_SEGMENT_DISTANCE:
            print(
                f"ERROR: Mission {mission_idx}, segment {i}->{i+1}: "
                f"Discontinuity detected! Distance={distance:.4f} (max allowed={MAX_SEGMENT_DISTANCE})"
            )
            print(f"  Point {i}: [{p1[0]:.4f}, {p1[1]:.4f}]")
            print(f"  Point {i+1}: [{p2[0]:.4f}, {p2[1]:.4f}]")
            return False

    return True


def validate_sea_crossings(path: List[List[float]], mission_idx: int) -> bool:
    """Validate that sea crossings only occur in allowed regions."""
    crossings_found = []

    for i in range(len(path) - 1):
        p1 = path[i]
        p2 = path[i + 1]
        distance = ((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2) ** 0.5

        if distance >= SEA_CROSSING_THRESHOLD:
            mid_u = (p1[0] + p2[0]) / 2
            mid_v = (p1[1] + p2[1]) / 2

            is_allowed = False
            crossing_name = None

            for name, (u_range_start, u_range_end) in ALLOWED_SEA_CROSSINGS.items():
                if u_range_start <= mid_u <= u_range_end:
                    is_allowed = True
                    crossing_name = name
                    break

            if is_allowed:
                crossings_found.append(
                    {
                        "segment": f"{i}->{i+1}",
                        "distance": distance,
                        "midpoint": (mid_u, mid_v),
                        "name": crossing_name,
                        "allowed": True,
                    }
                )
            else:
                print(
                    f"ERROR: Mission {mission_idx}, segment {i}->{i+1}: "
                    f"Unauthorized sea crossing detected!"
                )
                print(
                    f"  Distance: {distance:.4f} (threshold={SEA_CROSSING_THRESHOLD})"
                )
                print(f"  Start: [{p1[0]:.4f}, {p1[1]:.4f}]")
                print(f"  End:   [{p2[0]:.4f}, {p2[1]:.4f}]")
                print(f"  Midpoint: u={mid_u:.4f}, v={mid_v:.4f}")
                print(
                    f"  Allowed crossing regions: {list(ALLOWED_SEA_CROSSINGS.keys())}"
                )
                return False

    if crossings_found:
        print(
            f"Mission {mission_idx}: Found {len(crossings_found)} authorized sea crossing(s):"
        )
        for crossing in crossings_found:
            print(
                f"  - {crossing['name']} crossing at segment {crossing['segment']}: "
                f"{crossing['distance']:.4f} UV distance"
            )

    return True


def validate_all_paths(lines: List[List[List[float]]]) -> bool:
    """Validate all generated paths for continuity and sea crossings."""
    print("\n" + "=" * 70)
    print("VALIDATING GENERATED PATHS")
    print("=" * 70)

    all_valid = True

    for idx, path in enumerate(lines):
        print(f"\nValidating Mission {idx} ({len(path)} waypoints)...")

        if not validate_path_continuity(path, idx):
            all_valid = False
            continue
        else:
            print(
                f"  ✓ Path is continuous (all segments < {MAX_SEGMENT_DISTANCE} UV distance)"
            )

        if not validate_sea_crossings(path, idx):
            all_valid = False
            continue
        else:
            print("  ✓ All sea crossings are in authorized regions")

    print("\n" + "=" * 70)
    if all_valid:
        print("✓ ALL PATHS VALIDATED SUCCESSFULLY")
        print("=" * 70)
    else:
        print("✗ VALIDATION FAILED - See errors above")
        print("=" * 70)

    return all_valid


def simulate_cpp_rendering(lines: List[List[List[float]]]) -> bool:
    """Simulate C++ GL_LINE_STRIP rendering to verify paths will render correctly."""
    print("\n" + "=" * 70)
    print("SIMULATING C++ RENDERING (GL_LINE_STRIP)")
    print("=" * 70)

    all_valid = True

    for idx, path in enumerate(lines):
        if len(path) < 2:
            print(
                f"\nERROR: Mission {idx} cannot be rendered - need at least 2 vertices"
            )
            all_valid = False
            continue

        max_gap = 0.0
        total_length = 0.0

        for i in range(len(path) - 1):
            p1 = path[i]
            p2 = path[i + 1]
            segment_length = ((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2) ** 0.5
            total_length += segment_length
            max_gap = max(max_gap, segment_length)

        print(f"\nMission {idx} rendering analysis:")
        print(f"  Vertices: {len(path)}")
        print(f"  Line segments: {len(path) - 1}")
        print(f"  Total path length: {total_length:.4f} UV units")
        print(f"  Average segment: {total_length/(len(path)-1):.4f} UV units")
        print(f"  Max segment: {max_gap:.4f} UV units")

        if max_gap > MAX_SEGMENT_DISTANCE:
            print("  ✗ WARNING: Large gap detected (may appear discontinuous)")
            all_valid = False
        else:
            print("  ✓ Will render as continuous GL_LINE_STRIP")

    print("\n" + "=" * 70)
    if all_valid:
        print("✓ ALL PATHS WILL RENDER CORRECTLY IN C++")
    else:
        print("✗ SOME PATHS MAY HAVE RENDERING ISSUES")
    print("=" * 70)

    return all_valid


# The historical march, for the campaign map film (docs/CAMPAIGN_MAP_FILM.md).
#
# The eight mission lines above are the game's campaign route and are left
# exactly as they are. The march is a separate, named route that follows
# Polybius (III.33-118) and Livy (XXI-XXII, XXX): it starts at Carthago Nova,
# not Carthage, crosses the Alps instead of cutting from Massalia to
# Mediolanum, and runs on to Cannae, Capua, Croton and Zama. Every stop is a
# named place with real coordinates, so the film can reveal the line stop by
# stop, pin markers to battle sites and anchor army figures to the head.
#
# kind: city | battle | crossing | pass | waypoint. Waypoints shape the line
# and carry no label. leg: the kind of ground between this stop and the next.
MARCH_STOPS: List[dict] = [
    {
        "id": "carthago_nova",
        "name": "Carthago Nova",
        "lonlat": (-0.98, 37.60),
        "date": "Spring 218 BC",
        "kind": "city",
    },
    {
        "id": "saguntum",
        "name": "Saguntum",
        "lonlat": (-0.27, 39.68),
        "date": "219 BC",
        "kind": "battle",
    },
    {
        "id": "ebro",
        "name": "Ebro",
        "lonlat": (0.52, 40.81),
        "date": "Summer 218 BC",
        "kind": "crossing",
    },
    {
        "id": "pyrenees",
        "name": "Pyrenees",
        "lonlat": (2.87, 42.47),
        "date": "Summer 218 BC",
        "kind": "pass",
    },
    {
        "id": "ruscino",
        "name": "Ruscino",
        "lonlat": (2.90, 42.70),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "narbo",
        "name": "Narbo",
        "lonlat": (3.00, 43.18),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "baeterrae",
        "name": "Baeterrae",
        "lonlat": (3.22, 43.34),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "nemausus",
        "name": "Nemausus",
        "lonlat": (4.36, 43.84),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "rhone",
        "name": "Rhône Crossing",
        "lonlat": (4.78, 44.05),
        "date": "September 218 BC",
        "kind": "battle",
    },
    {
        "id": "the_island",
        "name": "The Island",
        "lonlat": (4.87, 44.97),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "cularo",
        "name": "Cularo",
        "lonlat": (5.72, 45.19),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "isere_valley",
        "name": "Isère valley",
        "lonlat": (6.05, 45.50),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "alps",
        "name": "The Alps",
        "lonlat": (6.93, 45.24),
        "date": "October 218 BC",
        "kind": "battle",
    },
    {
        "id": "taurasia",
        "name": "Taurasia",
        "lonlat": (7.68, 45.07),
        "date": "November 218 BC",
        "kind": "city",
    },
    {
        "id": "ticinus",
        "name": "Ticinus",
        "lonlat": (8.88, 45.20),
        "date": "November 218 BC",
        "kind": "battle",
    },
    {
        "id": "trebia",
        "name": "Trebia",
        "lonlat": (9.58, 45.02),
        "date": "December 218 BC",
        "kind": "battle",
    },
    {
        "id": "bononia",
        "name": "Bononia",
        "lonlat": (11.34, 44.49),
        "date": "Spring 217 BC",
        "kind": "waypoint",
    },
    {
        "id": "pistoria",
        "name": "Pistoria",
        "lonlat": (10.92, 43.93),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "arretium",
        "name": "Arretium",
        "lonlat": (11.88, 43.46),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "trasimene",
        "name": "Lake Trasimene",
        "lonlat": (12.08, 43.18),
        "date": "June 217 BC",
        "kind": "battle",
    },
    {
        "id": "spoletium",
        "name": "Spoletium",
        "lonlat": (12.74, 42.73),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "hadria",
        "name": "Hadria",
        "lonlat": (13.97, 42.66),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "gerunium",
        "name": "Gerunium",
        "lonlat": (14.97, 41.73),
        "date": "Winter 217 BC",
        "kind": "city",
    },
    {
        "id": "cannae",
        "name": "Cannae",
        "lonlat": (16.13, 41.31),
        "date": "2 August 216 BC",
        "kind": "battle",
    },
    {
        "id": "capua",
        "name": "Capua",
        "lonlat": (14.25, 41.08),
        "date": "216 BC",
        "kind": "city",
    },
    {
        "id": "tarentum",
        "name": "Tarentum",
        "lonlat": (17.24, 40.47),
        "date": "212 BC",
        "kind": "city",
    },
    {
        "id": "ad_tarentum",
        "name": "Tarentine hinterland",
        "lonlat": (17.05, 40.62),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "metapontum",
        "name": "Metapontum",
        "lonlat": (16.75, 40.45),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "thurii",
        "name": "Thurii",
        "lonlat": (16.42, 39.73),
        "date": "",
        "kind": "waypoint",
    },
    {
        "id": "croton",
        "name": "Croton",
        "lonlat": (17.12, 39.08),
        "date": "203 BC",
        "kind": "city",
        "leg": "open_sea",
    },
    {
        "id": "hadrumetum",
        "name": "Hadrumetum",
        "lonlat": (10.64, 35.83),
        "date": "203 BC",
        "kind": "city",
    },
    {
        "id": "zama",
        "name": "Zama",
        "lonlat": (9.40, 36.10),
        "date": "202 BC",
        "kind": "battle",
    },
]

MARCH_KINDS = {"city", "battle", "crossing", "pass", "waypoint"}


def build_march(bounds: dict) -> dict:
    """Build the named historical march as one polyline plus stop indices."""
    points: List[List[float]] = []
    stops: List[dict] = []
    for i, stop in enumerate(MARCH_STOPS):
        u, v = lon_lat_to_uv(stop["lonlat"][0], stop["lonlat"][1], bounds)
        stops.append(
            {
                "id": stop["id"],
                "name": stop["name"],
                "date": stop["date"],
                "kind": stop["kind"],
                "lonlat": [float(stop["lonlat"][0]), float(stop["lonlat"][1])],
                "uv": [float(u), float(v)],
                "index": len(points),
            }
        )
        if i + 1 == len(MARCH_STOPS):
            points.append([float(u), float(v)])
            break
        nxt = MARCH_STOPS[i + 1]
        nu, nv = lon_lat_to_uv(nxt["lonlat"][0], nxt["lonlat"][1], bounds)
        leg = stop.get("leg", "land")
        segment = add_coastal_waypoints((u, v), (nu, nv), leg, 1)
        for pu, pv in segment[:-1]:
            points.append([float(pu), float(pv)])
    return {
        "source": "Polybius III.33-118, Livy XXI-XXII and XXX",
        "points": points,
        "stops": stops,
    }


def validate_march(march: dict) -> bool:
    """The march must be continuous, inside the map and reference real points."""
    points = march["points"]
    stops = march["stops"]
    ok = True
    ids = set()
    for stop in stops:
        if stop["id"] in ids:
            print(f"ERROR: march stop id '{stop['id']}' is duplicated")
            ok = False
        ids.add(stop["id"])
        if stop["kind"] not in MARCH_KINDS:
            print(f"ERROR: march stop '{stop['id']}' has unknown kind {stop['kind']}")
            ok = False
        u, v = stop["uv"]
        if not (0.0 <= u <= 1.0 and 0.0 <= v <= 1.0):
            print(f"ERROR: march stop '{stop['id']}' lies outside the map bounds")
            ok = False
        index = stop["index"]
        if index >= len(points) or points[index] != stop["uv"]:
            print(f"ERROR: march stop '{stop['id']}' index does not land on its point")
            ok = False
    indices = [stop["index"] for stop in stops]
    if indices != sorted(indices) or len(set(indices)) != len(indices):
        print("ERROR: march stop indices must strictly increase")
        ok = False
    for i in range(len(points) - 1):
        p1, p2 = points[i], points[i + 1]
        distance = ((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2) ** 0.5
        if distance > MAX_SEGMENT_DISTANCE * 1.6:
            print(f"ERROR: march segment {i}->{i + 1} jumps {distance:.4f} UV")
            ok = False
    return ok


def main() -> None:
    """Generate and validate Hannibal's campaign paths."""
    print("Generating Hannibal's campaign paths...\n")

    bounds = json.loads(BOUNDS_PATH.read_text())
    lines = build_hannibal_path(bounds)

    if not lines:
        print("ERROR: Failed to generate any paths!")
        sys.exit(1)

    if not validate_all_paths(lines):
        print("\nERROR: Path validation failed!")
        print("Please fix the route definitions and try again.")
        sys.exit(1)

    if not simulate_cpp_rendering(lines):
        print("\nWARNING: Some paths may have rendering issues in C++")
        print("Review the warnings above and consider adjusting waypoint density.")

    march = build_march(bounds)
    if not validate_march(march):
        print("\nERROR: Historical march validation failed!")
        sys.exit(1)

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"lines": lines, "march": march}, indent=2))

    print(f"\n✓ Successfully wrote validated paths to {OUT_PATH}")
    print(f"  Total missions: {len(lines)}")
    print(f"  Total waypoints: {sum(len(line) for line in lines)}")
    print(
        f"  Historical march: {len(march['stops'])} stops, {len(march['points'])} points"
    )


if __name__ == "__main__":
    main()
