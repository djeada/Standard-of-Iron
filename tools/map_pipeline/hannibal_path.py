#!/usr/bin/env python3
"""Generate hannibal_path.json from provinces.json city UVs.

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

Usage:
    python3 tools/map_pipeline/hannibal_path.py

Requires:
    - assets/campaign_map/provinces.json with city UV data

Outputs:
    - assets/campaign_map/hannibal_path.json
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import List

ROOT = Path(__file__).resolve().parents[2]
PROVINCES_PATH = ROOT / "assets" / "campaign_map" / "provinces.json"
OUT_PATH = ROOT / "assets" / "campaign_map" / "hannibal_path.json"


def build_hannibal_path(provinces: List[dict]) -> List[List[List[float]]]:
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
    """
    city_uv: dict[str, List[float]] = {}
    for prov in provinces:
        for city in prov.get("cities", []):
            name = city.get("name")
            uv = city.get("uv")
            if not name or not uv or len(uv) < 2:
                continue
            city_uv[name] = [float(uv[0]), float(uv[1])]

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
                print(f"Warning: Mission {idx} - Hannibal path city '{name}' not found in provinces.")
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
    data = json.loads(PROVINCES_PATH.read_text())
    provinces = data.get("provinces", [])
    lines = build_hannibal_path(provinces)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps({"lines": lines}, indent=2))
    print(f"Wrote Hannibal path to {OUT_PATH}")


if __name__ == "__main__":
    main()
