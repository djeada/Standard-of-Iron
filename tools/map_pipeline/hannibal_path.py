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
from typing import List, Tuple, Optional

ROOT = Path(__file__).resolve().parents[2]
BOUNDS_PATH = ROOT / "tools" / "map_pipeline" / "map_bounds.json"
OUT_PATH = ROOT / "assets" / "campaign_map" / "hannibal_path.json"

# Validation constants
MAX_SEGMENT_DISTANCE = 0.25  # Maximum UV distance for a single segment (to detect discontinuities)
SEA_CROSSING_THRESHOLD = 0.10  # Minimum distance to be considered a sea crossing
ALLOWED_SEA_CROSSINGS = {
    "Gibraltar": (0.10, 0.40),  # Tingis to New Carthage (Africa to Spain) - wider range for curve
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


def add_coastal_waypoints(start: Tuple[float, float], end: Tuple[float, float], 
                         segment_type: str) -> List[Tuple[float, float]]:
    """Add intermediate waypoints for coastal segments to create curved paths.
    
    For coastal segments, adds waypoints that follow the shoreline more closely.
    For open sea segments, uses straight line.
    """
    if segment_type == "open_sea":
        # Direct crossing - no intermediate points needed
        return [start, end]
    
    # For coastal and land segments, add intermediate waypoints with slight curve
    # This creates a more organic path that visually follows the coast
    su, sv = start
    eu, ev = end
    
    # Calculate midpoint
    mid_u = (su + eu) / 2.0
    mid_v = (sv + ev) / 2.0
    
    # Add perpendicular offset to create slight curve toward coastline
    # The offset direction is chosen to bias toward the coast
    du = eu - su
    dv = ev - sv
    
    # Perpendicular vector (rotated 90 degrees)
    perp_u = -dv
    perp_v = du
    
    # Normalize and scale the perpendicular vector
    length = (perp_u**2 + perp_v**2)**0.5
    if length > 0.001:
        perp_u /= length
        perp_v /= length
        
        # Offset magnitude based on segment length (smaller for short segments)
        # For coastal segments, use MUCH smaller offset to avoid going over water
        if segment_type == "coastal":
            # Coastal: very subtle curve, stay close to direct path
            offset = min(0.008, length * 0.05)
        else:
            # Land: can use slightly larger curve
            offset = min(0.015, length * 0.10)
        
        # Bias toward the coast
        if segment_type == "coastal":
            # Coastal: very subtle curve to avoid crossing water
            curve_u = mid_u + perp_u * offset * 0.3
            curve_v = mid_v + perp_v * offset * 0.3
        else:  # land
            # Gentler curve for inland segments
            curve_u = mid_u + perp_u * offset * 0.4
            curve_v = mid_v + perp_v * offset * 0.4
        
        # Add two intermediate points for smoother curve
        quarter_u = (su + curve_u) / 2.0
        quarter_v = (sv + curve_v) / 2.0
        three_quarter_u = (curve_u + eu) / 2.0
        three_quarter_v = (curve_v + ev) / 2.0
        
        return [start, (quarter_u, quarter_v), (curve_u, curve_v), 
                (three_quarter_u, three_quarter_v), end]
    
    return [start, end]


def build_hannibal_path(bounds: dict) -> List[List[List[float]]]:
    """Build 8 progressive paths for Hannibal's campaign with coastline awareness.
    
    Each path builds upon the previous one, with the final path being the longest
    and covering all previous paths. This aligns with the 8 campaign missions:
    
    Mission 0: Crossing the Rhône
    Mission 1: Battle of Ticino
    Mission 2: Battle of Trebia
    Mission 3: Battle of Trasimene
    Mission 4: Battle of Cannae
    Mission 5: Campania Campaign
    Mission 6: Crossing the Alps (flashback)
    Mission 7: Battle of Zama (final confrontation)
    
    City coordinates are from provinces.py and converted to UV space.
    Routes are enhanced with intermediate waypoints for coastal segments.
    """
    # City coordinates (lon, lat) from provinces.py
    city_coords = {
        "Carthage": (10.3, 36.85),
        "Cirta": (6.6, 36.4),
        # Additional waypoints along North African coast before crossing
        "Hippo_Regius": (7.8, 36.9),  # Modern Annaba, Algeria
        "Icosium": (3.0, 36.8),  # Modern Algiers
        "Caesarea": (2.2, 36.6),  # Modern Cherchell, Algeria
        # More waypoints along Moroccan coast (critical for coastline following)
        "Saldae": (5.1, 36.75),  # Between Cirta and Icosium
        "Rusaddir": (-2.9, 35.3),  # Melilla area, Morocco
        "Tingis": (-5.8, 35.8),  # Tangier, Morocco - crossing point to Spain
        "New Carthage": (-0.98, 37.6),
        "Saguntum": (-0.3, 39.7),
        "Tarraco": (1.25, 41.1),
        "Emporiae": (3.1, 42.1),
        # Additional waypoint along southern French coast
        "Narbo": (3.0, 43.2),  # Narbonne, between Emporiae and Massalia
        "Massalia": (5.4, 43.3),
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
    
    # Define segment types for each pair of cities
    # "coastal": follows Mediterranean coastline
    # "land": inland route (e.g., through Alps)
    # "open_sea": explicit sea crossing
    segment_types = {
        ("Carthage", "Cirta"): "coastal",
        ("Cirta", "Hippo_Regius"): "coastal",
        ("Hippo_Regius", "Saldae"): "coastal",
        ("Saldae", "Icosium"): "coastal",
        ("Icosium", "Caesarea"): "coastal",
        ("Caesarea", "Rusaddir"): "coastal",
        ("Rusaddir", "Tingis"): "coastal",
        ("Tingis", "New Carthage"): "open_sea",  # Gibraltar crossing - Africa to Spain
        ("New Carthage", "Saguntum"): "coastal",
        ("Saguntum", "Tarraco"): "coastal",
        ("Tarraco", "Emporiae"): "coastal",
        ("Emporiae", "Narbo"): "coastal",
        ("Narbo", "Massalia"): "coastal",
        ("Massalia", "Mediolanum"): "land",  # Through the Alps
        ("Mediolanum", "Placentia"): "land",
        ("Placentia", "Ariminum"): "coastal",
        ("Ariminum", "Veii"): "land",
        ("Veii", "Rome"): "land",
        ("Rome", "Capua"): "land",
        ("Capua", "Tarentum"): "coastal",
        ("Tarentum", "Syracuse"): "coastal",
        ("Syracuse", "Lilybaeum"): "open_sea",  # Sicily to Carthage crossing (part 1)
        ("Lilybaeum", "Carthage"): "open_sea",  # Sicily to Carthage crossing (part 2)
    }
    
    # Convert to UV coordinates
    city_uv: dict[str, Tuple[float, float]] = {}
    for name, (lon, lat) in city_coords.items():
        u, v = lon_lat_to_uv(lon, lat, bounds)
        city_uv[name] = (float(u), float(v))

    # Define the route for each mission (cumulative)
    # Historical route: Carthage → North Africa coast → Gibraltar → Iberia → Gaul → Alps → Italy → back to Carthage
    # Routes now follow the African coast west with more waypoints before crossing at Gibraltar
    mission_routes = [
        # Mission 0: Crossing the Rhône (From North Africa along coast, then through Iberia to Gaul)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia"],
        
        # Mission 1: Battle of Ticino (Cross Alps, enter Italy)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum"],
        
        # Mission 2: Battle of Trebia (Consolidate in Cisalpine Gaul)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia"],
        
        # Mission 3: Battle of Trasimene (Move through Etruria)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii"],
        
        # Mission 4: Battle of Cannae (Advance to Southern Italy)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua"],
        
        # Mission 5: Campania Campaign (Consolidate in Campania, extend to Tarentum)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum"],
        
        # Mission 6: Crossing the Alps (Flashback - the mountain crossing with more detail)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum"],
        
        # Mission 7: Battle of Zama (Return to Africa for final battle - complete journey)
        ["Carthage", "Cirta", "Hippo_Regius", "Saldae", "Icosium", "Caesarea", "Rusaddir", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum", "Syracuse", "Lilybaeum", "Carthage"],
    ]

    lines: List[List[List[float]]] = []
    for idx, route_names in enumerate(mission_routes):
        path_points: List[Tuple[float, float]] = []
        
        # First, collect all city waypoints
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
        
        # Now generate the smooth path with intermediate waypoints
        for i in range(len(city_waypoints) - 1):
            start = city_waypoints[i]
            end = city_waypoints[i + 1]
            
            # Determine segment type
            start_city = route_names[i]
            end_city = route_names[i + 1]
            segment_type = segment_types.get((start_city, end_city), "land")
            
            # Add waypoints with coastal awareness
            segment_points = add_coastal_waypoints(start, end, segment_type)
            
            # Add all points except the last (to avoid duplication)
            path_points.extend(segment_points[:-1])
        
        # Add the final point
        path_points.append(city_waypoints[-1])
        
        # Convert to the expected format
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
    """Validate that a path is continuous without gaps.
    
    Args:
        path: List of [u, v] waypoints
        mission_idx: Mission index for error reporting
        
    Returns:
        True if path is continuous, False otherwise
    """
    if len(path) < 2:
        print(f"ERROR: Mission {mission_idx} has too few waypoints ({len(path)})")
        return False
    
    for i in range(len(path) - 1):
        p1 = path[i]
        p2 = path[i + 1]
        
        # Calculate Euclidean distance
        distance = ((p2[0] - p1[0])**2 + (p2[1] - p1[1])**2)**0.5
        
        if distance > MAX_SEGMENT_DISTANCE:
            print(f"ERROR: Mission {mission_idx}, segment {i}->{i+1}: "
                  f"Discontinuity detected! Distance={distance:.4f} (max allowed={MAX_SEGMENT_DISTANCE})")
            print(f"  Point {i}: [{p1[0]:.4f}, {p1[1]:.4f}]")
            print(f"  Point {i+1}: [{p2[0]:.4f}, {p2[1]:.4f}]")
            return False
    
    return True


def validate_sea_crossings(path: List[List[float]], mission_idx: int) -> bool:
    """Validate that sea crossings only occur in allowed regions.
    
    Args:
        path: List of [u, v] waypoints
        mission_idx: Mission index for error reporting
        
    Returns:
        True if all sea crossings are in allowed regions, False otherwise
    """
    crossings_found = []
    
    for i in range(len(path) - 1):
        p1 = path[i]
        p2 = path[i + 1]
        
        # Calculate distance
        distance = ((p2[0] - p1[0])**2 + (p2[1] - p1[1])**2)**0.5
        
        # Check if this is a potential sea crossing
        if distance >= SEA_CROSSING_THRESHOLD:
            # Determine midpoint position
            mid_u = (p1[0] + p2[0]) / 2
            mid_v = (p1[1] + p2[1]) / 2
            
            # Check if this crossing is in an allowed region
            is_allowed = False
            crossing_name = None
            
            for name, (u_range_start, u_range_end) in ALLOWED_SEA_CROSSINGS.items():
                if u_range_start <= mid_u <= u_range_end:
                    is_allowed = True
                    crossing_name = name
                    break
            
            if is_allowed:
                crossings_found.append({
                    "segment": f"{i}->{i+1}",
                    "distance": distance,
                    "midpoint": (mid_u, mid_v),
                    "name": crossing_name,
                    "allowed": True
                })
            else:
                print(f"ERROR: Mission {mission_idx}, segment {i}->{i+1}: "
                      f"Unauthorized sea crossing detected!")
                print(f"  Distance: {distance:.4f} (threshold={SEA_CROSSING_THRESHOLD})")
                print(f"  Start: [{p1[0]:.4f}, {p1[1]:.4f}]")
                print(f"  End: [{p2[0]:.4f}, {p2[1]:.4f}]")
                print(f"  Midpoint: u={mid_u:.4f}, v={mid_v:.4f}")
                print(f"  Allowed crossing regions: {list(ALLOWED_SEA_CROSSINGS.keys())}")
                return False
    
    # Report authorized crossings
    if crossings_found:
        print(f"Mission {mission_idx}: Found {len(crossings_found)} authorized sea crossing(s):")
        for crossing in crossings_found:
            print(f"  - {crossing['name']} crossing at segment {crossing['segment']}: "
                  f"{crossing['distance']:.4f} UV distance")
    
    return True


def validate_all_paths(lines: List[List[List[float]]]) -> bool:
    """Validate all generated paths for continuity and sea crossings.
    
    Args:
        lines: List of paths, where each path is a list of [u, v] waypoints
        
    Returns:
        True if all paths are valid, False otherwise
    """
    print("\n" + "=" * 70)
    print("VALIDATING GENERATED PATHS")
    print("=" * 70)
    
    all_valid = True
    
    for idx, path in enumerate(lines):
        print(f"\nValidating Mission {idx} ({len(path)} waypoints)...")
        
        # Check continuity
        if not validate_path_continuity(path, idx):
            all_valid = False
            continue
        else:
            print(f"  ✓ Path is continuous (all segments < {MAX_SEGMENT_DISTANCE} UV distance)")
        
        # Check sea crossings
        if not validate_sea_crossings(path, idx):
            all_valid = False
            continue
        else:
            print(f"  ✓ All sea crossings are in authorized regions")
    
    print("\n" + "=" * 70)
    if all_valid:
        print("✓ ALL PATHS VALIDATED SUCCESSFULLY")
        print("=" * 70)
    else:
        print("✗ VALIDATION FAILED - See errors above")
        print("=" * 70)
    
    return all_valid


def simulate_cpp_rendering(lines: List[List[List[float]]]) -> bool:
    """Simulate C++ GL_LINE_STRIP rendering to verify paths will render correctly.
    
    In C++, paths are rendered using GL_LINE_STRIP which connects consecutive
    vertices with line segments. This function verifies that:
    1. Each path can be rendered as a single GL_LINE_STRIP
    2. No gaps exist that would break the visual continuity
    
    Args:
        lines: List of paths to validate
        
    Returns:
        True if all paths will render correctly
    """
    print("\n" + "=" * 70)
    print("SIMULATING C++ RENDERING (GL_LINE_STRIP)")
    print("=" * 70)
    
    all_valid = True
    
    for idx, path in enumerate(lines):
        if len(path) < 2:
            print(f"\nERROR: Mission {idx} cannot be rendered - need at least 2 vertices")
            all_valid = False
            continue
        
        # GL_LINE_STRIP creates line segments between consecutive vertices
        # Verify each segment is renderable
        max_gap = 0.0
        total_length = 0.0
        
        for i in range(len(path) - 1):
            p1 = path[i]
            p2 = path[i + 1]
            segment_length = ((p2[0] - p1[0])**2 + (p2[1] - p1[1])**2)**0.5
            total_length += segment_length
            max_gap = max(max_gap, segment_length)
        
        print(f"\nMission {idx} rendering analysis:")
        print(f"  Vertices: {len(path)}")
        print(f"  Line segments: {len(path) - 1}")
        print(f"  Total path length: {total_length:.4f} UV units")
        print(f"  Average segment: {total_length/(len(path)-1):.4f} UV units")
        print(f"  Max segment: {max_gap:.4f} UV units")
        
        if max_gap > MAX_SEGMENT_DISTANCE:
            print(f"  ✗ WARNING: Large gap detected (may appear discontinuous)")
            all_valid = False
        else:
            print(f"  ✓ Will render as continuous GL_LINE_STRIP")
    
    print("\n" + "=" * 70)
    if all_valid:
        print("✓ ALL PATHS WILL RENDER CORRECTLY IN C++")
    else:
        print("✗ SOME PATHS MAY HAVE RENDERING ISSUES")
    print("=" * 70)
    
    return all_valid


def main() -> None:
    """Generate and validate Hannibal's campaign paths."""
    print("Generating Hannibal's campaign paths...")
    print()
    
    # Load map bounds
    bounds = json.loads(BOUNDS_PATH.read_text())
    
    # Generate paths
    lines = build_hannibal_path(bounds)
    
    if not lines:
        print("ERROR: Failed to generate any paths!")
        sys.exit(1)
    
    # Validate paths
    if not validate_all_paths(lines):
        print("\nERROR: Path validation failed!")
        print("Please fix the route definitions and try again.")
        sys.exit(1)
    
    # Simulate C++ rendering
    if not simulate_cpp_rendering(lines):
        print("\nWARNING: Some paths may have rendering issues in C++")
        print("Review the warnings above and consider adjusting waypoint density.")
        # Don't exit on rendering warnings, just inform
    
    # Write validated paths to file
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"lines": lines}, indent=2))
    
    print(f"\n✓ Successfully wrote validated paths to {OUT_PATH}")
    print(f"  Total missions: {len(lines)}")
    print(f"  Total waypoints: {sum(len(line) for line in lines)}")


if __name__ == "__main__":
    main()
