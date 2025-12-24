#!/usr/bin/env python3
"""Generate hannibal_path.json from city lon/lat coordinates.

This script generates 8 progressive paths for Hannibal's campaign, one for each
mission in the Second Punic War campaign. Each path builds upon the previous,
with the final path being the longest and covering all previous waypoints.

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
    - assets/campaign_map/hannibal_path.json
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


def build_hannibal_path(bounds: dict) -> List[List[List[float]]]:
    """Build 8 progressive paths for Hannibal's campaign, one for each mission.
    
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
    """
    # City coordinates (lon, lat) from provinces.py
    city_coords = {
        "Carthage": (10.3, 36.85),
        "Cirta": (6.6, 36.4),
        "Tingis": (-5.8, 35.8),
        "New Carthage": (-0.98, 37.6),
        "Saguntum": (-0.3, 39.7),
        "Tarraco": (1.25, 41.1),
        "Emporiae": (3.1, 42.1),
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
    
    # Convert to UV coordinates
    city_uv: dict[str, List[float]] = {}
    for name, (lon, lat) in city_coords.items():
        u, v = lon_lat_to_uv(lon, lat, bounds)
        city_uv[name] = [float(u), float(v)]

    # Define the route for each mission (cumulative)
    # Historical route: Carthage → North Africa/Numidia → Iberia → Gaul → Alps → Italy → back to Carthage
    # Added intermediate waypoints to avoid path going through sea
    mission_routes = [
        # Mission 0: Crossing the Rhône (From North Africa through Iberia to Gaul)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia"],
        
        # Mission 1: Battle of Ticino (Cross Alps, enter Italy)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum"],
        
        # Mission 2: Battle of Trebia (Consolidate in Cisalpine Gaul)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum", "Placentia"],
        
        # Mission 3: Battle of Trasimene (Move through Etruria)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii"],
        
        # Mission 4: Battle of Cannae (Advance to Southern Italy)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua"],
        
        # Mission 5: Campania Campaign (Consolidate in Campania, extend to Tarentum)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum"],
        
        # Mission 6: Crossing the Alps (Flashback - the mountain crossing with more detail)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum"],
        
        # Mission 7: Battle of Zama (Return to Africa for final battle - complete journey)
        ["Carthage", "Cirta", "Tingis", "New Carthage", "Saguntum", "Tarraco", "Emporiae", "Massalia", "Mediolanum", "Placentia", "Ariminum", "Veii", "Rome", "Capua", "Tarentum", "Syracuse", "Lilybaeum", "Carthage"],
    ]

    lines: List[List[List[float]]] = []
    for idx, route_names in enumerate(mission_routes):
        line: List[List[float]] = []
        for name in route_names:
            uv = city_uv.get(name)
            if uv is None:
                print(f"Warning: Mission {idx} - Hannibal path city '{name}' not found.")
                continue
            line.append(uv)

        if len(line) >= 2:
            lines.append(line)
        else:
            print(f"Warning: Mission {idx} has insufficient waypoints ({len(line)}), skipping.")

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
