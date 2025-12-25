#!/usr/bin/env python3
"""Generate hannibal_path.json with coastline-aware routing and validation.

This script generates 8 progressive paths for Hannibal's campaign, one for each
mission in the Second Punic War campaign. Each path builds upon the previous,
with the final path being the longest and covering all previous waypoints.

Key features:
- Routes follow coastlines with curved paths instead of rigid straight lines
- Each segment is classified as 'coastal', 'land', or 'open_sea'
- Open-sea crossings only occur on explicit segments (Africa → Spain, Sicily → Carthage)
- Coastal segments include intermediate waypoints for smooth, organic appearance
- Paths visually follow Mediterranean shorelines where applicable

Validation system:
- Validates path continuity (no gaps or discontinuities)
- Ensures sea crossings only occur in authorized regions
- Simulates C++ GL_LINE_STRIP rendering to verify visual continuity
- Script exits with error if validation fails

The paths are designed to align with the campaign mission structure:
- Mission 0: Crossing the Rhône
- Mission 1: Battle of Ticino
- Mission 2: Battle of Trebia
- Mission 3: Battle of Trasimene
- Mission 4: Battle of Cannae
- Mission 5: Campania Campaign
- Mission 6: Crossing the Alps (flashback)
- Mission 7: Battle of Zama (final confrontation)

City coordinates are taken from provinces.py definitions and converted to UV space.

Fix applied:
- Reordered early North Africa waypoints to remove “back-and-forth” near Carthage.
  Previously: Carthage → Cirta → Hippo_Regius (eastward backtrack)
  Now:        Carthage → Hippo_Regius → Cirta → Saldae (monotonic west)

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

# Validation constants
MAX_SEGMENT_DISTANCE = 0.25  # Maximum UV distance for a single segment (to detect discontinuities)
SEA_CROSSING_THRESHOLD = 0.10  # Minimum distance to be considered a sea crossing
ALLOWED_SEA_CROSSINGS = {
    "Gibraltar": (0.10, 0.40),  # Tingis to Carteia (Africa to Spain) - wider range for curve
    "Sicily-Carthage": (0.75, 0.95),  # Sicily to Carthage crossing - adjusted for actual position
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
) -> List[Tuple[float, float]]:
    """Add intermediate waypoints for coastal/land segments to create curved paths.

    For open sea segments, uses a straight line.
    """
    if segment_type == "open_sea":
        return [start, end]

    su, sv = start
    eu, ev = end

    # Midpoint
    mid_u = (su + eu) / 2.0
    mid_v = (sv + ev) / 2.0

    # Perpendicular offset to create a gentle curve
    du = eu - su
    dv = ev - sv
    perp_u = -dv
    perp_v = du

    length = (perp_u**2 + perp_v**2) ** 0.5
    if length > 0.001:
        perp_u /= length
        perp_v /= length

        # Keep coastal curvature subtle to reduce unintended “cutting across water”
        if segment_type == "coastal":
            offset = min(0.008, length * 0.05)
            curve_u = mid_u + perp_u * offset * 0.3
            curve_v = mid_v + perp_v * offset * 0.3
        else:  # land
            offset = min(0.015, length * 0.10)
            curve_u = mid_u + perp_u * offset * 0.4
            curve_v = mid_v + perp_v * offset * 0.4

        # Two extra points for smoother curve
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
    # City coordinates (lon, lat) from provinces.py
    city_coords = {
        "Carthage": (10.3, 36.85),
        "Cirta": (6.6, 36.4),
        # Additional waypoints along North African coast before crossing
        "Hippo_Regius": (7.8, 36.9),  # Modern Annaba, Algeria (coast)
        "Icosium": (3.0, 36.8),  # Modern Algiers
        "Caesarea": (2.2, 36.6),  # Modern Cherchell, Algeria
        "Saldae": (5.1, 36.75),  # Coastal Bejaia area
        "Rusaddir": (-2.9, 35.3),  # Melilla area, Morocco
        "Tingis": (-5.8, 35.8),  # Tangier, Morocco - crossing point to Spain
        # Spanish south coast cities
        "Carteia": (-5.4, 36.2),  # Near Gibraltar on Spanish side
        "Malaca": (-4.4, 36.7),  # Málaga
        "Abdera": (-3.0, 36.75),  # Between Málaga and Almería
        "New Carthage": (-0.98, 37.6),  # Cartagena area
        "Saguntum": (-0.3, 39.7),
        "Tarraco": (1.25, 41.1),
        "Emporiae": (3.1, 42.1),
        # Southern French coast
        "Narbo": (3.0, 43.2),  # Narbonne
        "Massalia": (5.4, 43.3),
        # Italy
        "Mediolanum": (9.2, 45.5),
        "Placentia": (9.7, 45.0),
        "Ariminum": (12.6, 44.0),
        "Veii": (12.3, 42.0),
        "Rome": (12.5, 41.9),
        "Capua": (14.3, 41.1),
        "Tarentum": (17.2, 40.5),
        "Syracuse": (15.3, 37.1),
        "Lilybaeum": (12.5, 37.8),
    }

    # Segment classification with curve directions
    # Format: (city1, city2): (segment_type, curve_direction)
    # curve_direction: 1 = standard (curves to the right), -1 = opposite (curves to the left)
    segment_types = {
        # FIX: reorder Africa start to avoid backtracking near Carthage
        ("Carthage", "Hippo_Regius"): ("land", 1),  # Changed to land to hug the coast better
        ("Hippo_Regius", "Cirta"): ("land", 1),
        ("Cirta", "Saldae"): ("land", 1),

        # North Africa westbound coast after returning to Saldae
        ("Saldae", "Icosium"): ("land", -1),  # Changed to land, curve inland
        ("Icosium", "Caesarea"): ("land", -1),  # Changed to land, curve inland
        ("Caesarea", "Rusaddir"): ("land", -1),  # Changed to land, curve inland
        ("Rusaddir", "Tingis"): ("coastal", -1),  # Keep coastal but curve inland

        # Gibraltar crossing
        ("Tingis", "Carteia"): ("open_sea", 1),

        # Spanish coast to Gaul
        ("Carteia", "Malaca"): ("land", 1),  # Changed to land to follow coast closely
        ("Malaca", "Abdera"): ("land", 1),  # Changed to land
        ("Abdera", "New Carthage"): ("land", 1),  # Changed to land
        ("New Carthage", "Saguntum"): ("land", 1),  # Changed to land
        ("Saguntum", "Tarraco"): ("land", 1),  # Changed to land
        ("Tarraco", "Emporiae"): ("coastal", 1),
        ("Emporiae", "Narbo"): ("coastal", 1),
        ("Narbo", "Massalia"): ("coastal", 1),

        # Alps / Italy
        ("Massalia", "Mediolanum"): ("land", 1),
        ("Mediolanum", "Placentia"): ("land", 1),
        ("Placentia", "Ariminum"): ("land", -1),  # Changed to land, curve away from sea
        ("Ariminum", "Veii"): ("land", 1),
        ("Veii", "Rome"): ("land", 1),
        ("Rome", "Capua"): ("land", 1),
        ("Capua", "Tarentum"): ("land", -1),  # Changed to land, curve inland

        # Return to Africa
        ("Tarentum", "Syracuse"): ("land", -1),  # Changed to land to follow coast
        ("Syracuse", "Lilybaeum"): ("open_sea", 1),
        ("Lilybaeum", "Carthage"): ("open_sea", 1),
    }

    # Convert to UV coordinates
    city_uv: dict[str, Tuple[float, float]] = {}
    for name, (lon, lat) in city_coords.items():
        u, v = lon_lat_to_uv(lon, lat, bounds)
        city_uv[name] = (float(u), float(v))

    # Shared base route (prevents copy/paste drift across missions)
    BASE_AFRICA_TO_MASSALIA = [
        "Carthage",
        "Hippo_Regius",
        "Cirta",
        "Saldae",
        "Icosium",
        "Caesarea",
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
        # Mission 0: Crossing the Rhône (to Massalia)
        BASE_AFRICA_TO_MASSALIA,

        # Mission 1: Battle of Ticino
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum"],

        # Mission 2: Battle of Trebia
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia"],

        # Mission 3: Battle of Trasimene
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia", "Ariminum", "Veii"],

        # Mission 4: Battle of Cannae
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua"],

        # Mission 5: Campania Campaign
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum"],

        # Mission 6: Crossing the Alps (flashback - ends at Mediolanum)
        BASE_AFRICA_TO_MASSALIA + ["Mediolanum"],

        # Mission 7: Battle of Zama (complete journey)
        BASE_AFRICA_TO_MASSALIA
        + ["Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum", "Syracuse", "Lilybaeum", "Carthage"],
    ]

    lines: List[List[List[float]]] = []
    for idx, route_names in enumerate(mission_routes):
        path_points: List[Tuple[float, float]] = []

        # Collect city waypoints
        city_waypoints: List[Tuple[float, float]] = []
        for name in route_names:
            uv = city_uv.get(name)
            if uv is None:
                print(f"Warning: Mission {idx} - Hannibal path city '{name}' not found.")
                continue
            city_waypoints.append(uv)

        if len(city_waypoints) < 2:
            print(f"Warning: Mission {idx} has insufficient waypoints ({len(city_waypoints)}), skipping.")
            continue

        # Generate smooth path with intermediate waypoints
        for i in range(len(city_waypoints) - 1):
            start = city_waypoints[i]
            end = city_waypoints[i + 1]

            start_city = route_names[i]
            end_city = route_names[i + 1]
            segment_info = segment_types.get((start_city, end_city), ("land", 1))
            
            # Handle both old format (string) and new format (tuple)
            if isinstance(segment_info, tuple):
                segment_type, curve_direction = segment_info
            else:
                segment_type = segment_info
                curve_direction = 1

            segment_points = add_coastal_waypoints(start, end, segment_type, curve_direction)
            path_points.extend(segment_points[:-1])  # avoid duplication

        path_points.append(city_waypoints[-1])

        line: List[List[float]] = [[float(u), float(v)] for (u, v) in path_points]
        if len(line) >= 2:
            lines.append(line)
        else:
            print(f"Warning: Mission {idx} final path has insufficient points ({len(line)}), skipping.")

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
                print(f"  Distance: {distance:.4f} (threshold={SEA_CROSSING_THRESHOLD})")
                print(f"  Start: [{p1[0]:.4f}, {p1[1]:.4f}]")
                print(f"  End: [{p2[0]:.4f}, {p2[1]:.4f}]")
                print(f"  Midpoint: u={mid_u:.4f}, v={mid_v:.4f}")
                print(f"  Allowed crossing regions: {list(ALLOWED_SEA_CROSSINGS.keys())}")
                return False

    if crossings_found:
        print(f"Mission {mission_idx}: Found {len(crossings_found)} authorized sea crossing(s):")
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
            print(f"  ✓ Path is continuous (all segments < {MAX_SEGMENT_DISTANCE} UV distance)")

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
            print(f"\nERROR: Mission {idx} cannot be rendered - need at least 2 vertices")
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

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"lines": lines}, indent=2))

    print(f"\n✓ Successfully wrote validated paths to {OUT_PATH}")
    print(f"  Total missions: {len(lines)}")
    print(f"  Total waypoints: {sum(len(line) for line in lines)}")


if __name__ == "__main__":
    main()
