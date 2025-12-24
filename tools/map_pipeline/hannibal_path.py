#!/usr/bin/env python3
"""Generate hannibal_path.json with coastline-aware routing.

This script generates 8 progressive paths for Hannibal's campaign, one for each
mission in the Second Punic War campaign. Each path builds upon the previous,
with the final path being the longest and covering all previous waypoints.

Key improvements:
- Routes follow coastlines with curved paths instead of rigid straight lines
- Each segment is classified as 'coastal', 'land', or 'open_sea'
- Open-sea crossings only occur on explicit segments (Africa → Spain, Sicily → Carthage)
- Coastal segments include intermediate waypoints for smooth, organic appearance
- Paths visually follow Mediterranean shorelines where applicable

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
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import List, Tuple

ROOT = Path(__file__).resolve().parents[2]
BOUNDS_PATH = ROOT / "tools" / "map_pipeline" / "map_bounds.json"
OUT_PATH = ROOT / "assets" / "campaign_map" / "hannibal_path.json"


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
        offset = min(0.02, length * 0.15)
        
        # Bias toward the coast (negative for Mediterranean north coast)
        if segment_type == "coastal":
            # Curve slightly toward the sea side for coastal routes
            curve_u = mid_u + perp_u * offset * 0.5
            curve_v = mid_v + perp_v * offset * 0.5
        else:  # land
            # Gentler curve for inland segments
            curve_u = mid_u + perp_u * offset * 0.3
            curve_v = mid_v + perp_v * offset * 0.3
        
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
        ("Hippo_Regius", "Icosium"): "coastal",
        ("Icosium", "Caesarea"): "coastal",
        ("Caesarea", "Tingis"): "coastal",
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
    # Routes now follow the African coast west before crossing at Gibraltar
    mission_routes = [
        # Mission 0: Crossing the Rhône (From North Africa along coast, then through Iberia to Gaul)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia"],
        
        # Mission 1: Battle of Ticino (Cross Alps, enter Italy)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum"],
        
        # Mission 2: Battle of Trebia (Consolidate in Cisalpine Gaul)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia"],
        
        # Mission 3: Battle of Trasimene (Move through Etruria)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii"],
        
        # Mission 4: Battle of Cannae (Advance to Southern Italy)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua"],
        
        # Mission 5: Campania Campaign (Consolidate in Campania, extend to Tarentum)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum"],
        
        # Mission 6: Crossing the Alps (Flashback - the mountain crossing with more detail)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum"],
        
        # Mission 7: Battle of Zama (Return to Africa for final battle - complete journey)
        ["Carthage", "Cirta", "Hippo_Regius", "Icosium", "Caesarea", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Narbo", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum", "Syracuse", "Lilybaeum", "Carthage"],
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


def main() -> None:
    bounds = json.loads(BOUNDS_PATH.read_text())
    lines = build_hannibal_path(bounds)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"lines": lines}, indent=2))
    print(f"Wrote Hannibal path to {OUT_PATH}")


if __name__ == "__main__":
    main()
